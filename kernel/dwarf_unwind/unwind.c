#include <linux/module.h>
#include <linux/compiler.h>
#include <linux/debugfs.h>
#include <linux/ptrace.h>
#include <linux/dwarf_unwind.h>
#include <linux/uaccess.h>
#include <linux/hardirq.h>
#include <asm/dwarf_unwind_regs.h>
#include "internal.h"

unsigned int dwarf_unwind_debug = 0;
module_param(dwarf_unwind_debug, int, 0644);
MODULE_PARM_DESC(dwarf_unwind_debug, "Turns on debug for dwarf unwind code.");

struct unwind {
	struct pt_regs regs;
};

static void unwind_init(struct unwind *u, struct pt_regs *regs)
{
	memcpy(&u->regs, regs, sizeof(*regs));
}

#define NR_CONTEXTS 3

struct state_cpu {
	struct du_state state[NR_CONTEXTS];
	int recursion[NR_CONTEXTS];
};

static DEFINE_PER_CPU(struct state_cpu, state_cpu);

struct du_state *state_get(int *pctx)
{
	struct du_state *state;
	int ctx = NR_CONTEXTS;

	if (in_nmi())
		ctx = 0;
	else if (in_irq())
		ctx = 1;
	else if (in_softirq())
		ctx = 2;

	DU_DEBUG_UNWIND("ctx %d\n", ctx);

	if (ctx < NR_CONTEXTS) {
		struct state_cpu *st;

		get_cpu();
		st = &__get_cpu_var(state_cpu);

		DU_DEBUG_UNWIND("recursion %d\n", st->recursion[ctx]);

		if (st->recursion[ctx]) {
			put_cpu();
			return NULL;
		}

		st->recursion[ctx]++;
	        barrier();

		state = &st->state[ctx];
		memset(state, 0x0, sizeof(*state));
	} else
		state = kzalloc(sizeof(*state), GFP_ATOMIC);

	/*
	 * Note zeroing du_state means initialized
	 * to DU_LOCATION_SAME state.
	 */

	DU_DEBUG_UNWIND("state %p\n", state);

	*pctx = ctx;
	return state;
}

void state_put(struct du_state *state, int ctx)
{
	DU_DEBUG_UNWIND("ctx %d, state %p\n", ctx, state);

	if (ctx < NR_CONTEXTS) {
		struct state_cpu *st = &__get_cpu_var(state_cpu);

		st->recursion[ctx]--;
		barrier();

		DU_DEBUG_UNWIND("recursion %d\n", st->recursion[ctx]);

		put_cpu();
	} else
		kfree(state);
}

static int process_fde(struct du_fde *fde, struct du_state *state,
		       unsigned long ip)
{
	struct du_cie *cie = fde->cie;
	struct du_state_regs *state_current;
	int ret;

	ret = du_cfi(fde, state, ip, &cie->frame);
	if (ret)
		return ret;

	state_current = &state->state_current[state->cur];

	memcpy(&state->state_initial, state_current,
	       sizeof(*state_current));

	return du_cfi(fde, state, ip, &fde->frame);
}

static int __apply_state(struct du_regs *regs,
				struct du_state_regs *state)
{
	struct du_state_reg *cfa_state;
	struct du_regs tmp_regs;
	unsigned long prev_ip;
	unsigned long prev_cfa;
	unsigned long cfa;
	unsigned long expr_len;
	u8 *expr;
	int i;

	cfa_state = &state->reg[DU_REG_CFA_REG_COLUMN];

	prev_ip  = regs->reg[DU_REG_IP];
	prev_cfa = regs->reg[DU_REG_CFA];

	DU_DEBUG_UNWIND("prev_cfa 0x%lx, prev_ip 0x%lx\n",
			prev_cfa, prev_ip);

	if (cfa_state->loc == DU_LOCATION_REG) {
		struct du_state_reg *sp_state;

		/*
		 * CFA is equal to [reg] + offset:
		 *
		 * As a special-case, if the stack-pointer is the CFA and the
		 * stack-pointer wasn't saved, popping the CFA implicitly pops
		 * the stack-pointer as well.
		 */

		sp_state = &state->reg[DU_REG_SP];

		if ((cfa_state->val == DU_REG_SP) &&
		    (sp_state->loc == DU_LOCATION_SAME))
			cfa = prev_cfa;
		else {
			unsigned long reg;

			reg = cfa_state->val;

			DU_DEBUG_UNWIND("cfa reg %ld, val 0x%lx\n",
					cfa_state->val, regs->reg[reg]);

			cfa = regs->reg[reg];
		}

		cfa += state->reg[DU_REG_CFA_OFF_COLUMN].val;

		DU_DEBUG_UNWIND("cfa %lx += off 0x%lx\n",
				cfa, state->reg[DU_REG_CFA_OFF_COLUMN].val);

	} else {
		if ((cfa_state->loc != DU_LOCATION_EXPR) ||
		    (cfa_state->loc != DU_LOCATION_EXPR_VALUE))
			return -EINVAL;

		DU_DEBUG_UNWIND("cfa expr\n");

		expr     = cfa_state->expr;
		expr_len = cfa_state->len;

		if (du_expr(regs, expr, expr_len, &cfa))
			return -EINVAL;
	}

	regs->reg[DU_REG_CFA] = cfa;

	/* Suck new register values. */
	for (i = 0; i < DU_REGS_NUM; ++i) {
		if (state->reg[i].loc == DU_LOCATION_REG) {
			int reg = state->reg[i].val;
			tmp_regs.reg[i] = regs->reg[reg];
		}
	}

	/* And the rest. */
	for (i = 0; i < DU_REGS_NUM; ++i) {
		struct du_state_reg *rs = &state->reg[i];
		unsigned long *p;
		unsigned long val;

		switch (rs->loc) {
		case DU_LOCATION_REG:
			regs->reg[i] = tmp_regs.reg[i];
			break;

		case DU_LOCATION_SAME:
			break;

		case DU_LOCATION_MEMORY:
			p = (unsigned long *) (cfa + rs->val);

			if (probe_kernel_address(p, val)) {
				DU_DEBUG_UNWIND("LOC MEMORY failed %p\n", p);
				return -EFAULT;
			}

			DU_DEBUG_UNWIND("LOC MEMORY reg %d, cfa 0x%lx + 0x%lx [%p] = 0x%lx\n",
					i, cfa, rs->val, p, val);

			regs->reg[i] = val;
			break;

		case DU_LOCATION_EXPR:
		case DU_LOCATION_EXPR_VALUE:
			expr     = rs->expr;
			expr_len = rs->len;

			if (du_expr(regs, expr, expr_len, &val))
				return -EINVAL;

			if (rs->loc == DU_LOCATION_EXPR)
				regs->reg[i] = *((unsigned long *) val);
			else
				regs->reg[i] = val;

		case DU_LOCATION_UNDEF:
			regs->reg[i] = 0;

		case DU_LOCATION_VALUE:
			break;
		}
	}

	DU_DEBUG_UNWIND("cfa 0x%lx, ip 0x%lx\n", cfa, regs->reg[DU_REG_IP]);

	/* No change, too bad.. */
	if ((regs->reg[DU_REG_IP] == prev_ip) &&
	    (cfa == prev_cfa))
		return -EINVAL;

	return 0;
}

static int apply_state(struct unwind *u,
			      struct du_state *state)
{
	struct du_regs regs;
	struct du_state_regs *state_regs;
	int ret;

	du_arch_regs_get(&regs, &u->regs);

	state_regs = &state->state_current[state->cur];

	ret = __apply_state(&regs, state_regs);
	if (!ret)
		du_arch_regs_set(&regs, &u->regs);

	return ret;
}

static struct du_frames* frames_get(unsigned long addr)
{
	bool is_core = core_kernel_text(addr);
	struct module *mod = NULL;

	preempt_disable();

	if (!is_core)
		mod = __module_text_address(addr);

	preempt_enable();

	DU_DEBUG_UNWIND("is_core %d, mod %p\n", is_core, mod);

	/*
	 * It's not in core, but no module was found,
	 * do not search core frames.
	 */
	if (!is_core && !mod)
		return NULL;

	return du_frames_find(mod);
}

static int unwind_step(struct unwind *u)
{
	unsigned long addr = instruction_pointer(&u->regs);
	struct du_frames *frames;
	struct du_fde *fde;
	struct du_state *state;
	int ret = -EINVAL, ctx;

	DU_DEBUG_UNWIND("addr 0x%lx\n", addr);

	frames = frames_get(addr);
	if (!frames)
		goto out;

	DU_DEBUG_UNWIND("frames %p\n", frames);

	fde = du_fde_lookup(frames, addr);
	if (!fde)
		goto out_frames;

	DU_DEBUG_UNWIND("fde %p\n", fde);

	state = state_get(&ctx);
	if (!state)
		goto out_frames;

	ret = process_fde(fde, state, addr);
	if (ret)
		goto out_state;

	ret = apply_state(u, state);

 out_state:
	state_put(state, ctx);
 out_frames:
	du_frames_put(frames);
 out:
	DU_DEBUG_UNWIND("ret %d\n", ret);
	return ret;
}

int dwarf_unwind(struct pt_regs *regs, dwarf_unwind_cb cb, void *data)
{
	struct unwind u;
	int ret = 0;

	unwind_init(&u, regs);

	while (!ret && !unwind_step(&u))
		ret = cb(&u.regs, data);

	return ret;
}

static ssize_t
test_write(struct file *filp, const char __user *ubuf,
	   size_t cnt, loff_t *ppos)
{
	printk("Testing dwarf unwind from process context.\n");

	if (dwarf_unwind_debug) {
		printk("Dwarf unwind debug enabled: ");

		if (dwarf_unwind_debug & DU_DEBUG_READ)
			printk("read ");
		if (dwarf_unwind_debug & DU_DEBUG_FRAMES)
			printk("frames ");
		if (dwarf_unwind_debug & DU_DEBUG_EH_FRAMES)
			printk("eh-frames ");
		if (dwarf_unwind_debug & DU_DEBUG_CFI)
			printk("cfi ");
		if (dwarf_unwind_debug & DU_DEBUG_EXPR)
			printk("expr");

		printk("\n");
	}

	dump_stack();
	return cnt;
}

static const struct file_operations test_fops = {
	.write = test_write,
};

static int __init unwind_init_test(void)
{
	if (!debugfs_create_file("unwind_test", 0644, NULL, NULL,
				 &test_fops))
		return -ENOMEM;

	return 0;
}

late_initcall(unwind_init_test);
