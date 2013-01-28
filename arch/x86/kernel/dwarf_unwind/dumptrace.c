
#include <linux/dwarf_unwind.h>
#include <asm/stacktrace.h>

struct trace_data {
	const struct stacktrace_ops *ops;
	void *data;
};

static int unwind_trace(struct pt_regs *regs, void *data)
{
	struct trace_data *td = data;
	const struct stacktrace_ops *ops = td->ops;

	ops->address(td->data, regs->ip, 1);

	return 0;
}

void dump_trace_dwarf(struct task_struct *task, struct pt_regs *regs,
		      unsigned long *stack, unsigned long bp __maybe_unused,
		      const struct stacktrace_ops *ops, void *data)
{
	struct trace_data td = {
		.ops	= ops,
		.data	= data,
	};
	struct pt_regs regs_tmp;
	unsigned long dummy;

	if (!task)
		task = current;

	if (!stack) {
		if (regs)
			stack = (unsigned long *) regs->sp;
		else if (task != current)
			stack = (unsigned long *) task->thread.sp;
		else
			stack = &dummy;
	}

	if (!regs) {
		regs_load(&regs_tmp);
		regs = &regs_tmp;
		regs->sp = (unsigned long) stack;
	}

	dwarf_unwind(regs, unwind_trace, &td);
}
