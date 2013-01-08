
#include "internal.h"

#define NUM_OPERANDS(signature)	(((signature) >> 6) & 0x3)
#define OPND1_TYPE(signature)	(((signature) >> 3) & 0x7)
#define OPND2_TYPE(signature)	(((signature) >> 0) & 0x7)

#define OPND_SIGNATURE(n, t1, t2) (((n) << 6) | ((t1) << 3) | ((t2) << 0))
#define OPND1(t1)	OPND_SIGNATURE(1, t1, 0)
#define OPND2(t1, t2)	OPND_SIGNATURE(2, t1, t2)

#define VAL8	0x0
#define VAL16	0x1
#define VAL32	0x2
#define VAL64	0x3
#define ULEB128	0x4
#define SLEB128	0x5
#define OFFSET	0x6	/* 32-bit offset for 32-bit DWARF, 64-bit otherwise */
#define ADDR	0x7	/* Machine address.  */

enum {
	DW_OP_addr			= 0x03,
	DW_OP_deref			= 0x06,
	DW_OP_const1u			= 0x08,
	DW_OP_const1s			= 0x09,
	DW_OP_const2u			= 0x0a,
	DW_OP_const2s			= 0x0b,
	DW_OP_const4u			= 0x0c,
	DW_OP_const4s			= 0x0d,
	DW_OP_const8u			= 0x0e,
	DW_OP_const8s			= 0x0f,
	DW_OP_constu			= 0x10,
	DW_OP_consts			= 0x11,
	DW_OP_dup			= 0x12,
	DW_OP_drop			= 0x13,
	DW_OP_over			= 0x14,
	DW_OP_pick			= 0x15,
	DW_OP_swap			= 0x16,
	DW_OP_rot			= 0x17,
	DW_OP_xderef			= 0x18,
	DW_OP_abs			= 0x19,
	DW_OP_and			= 0x1a,
	DW_OP_div			= 0x1b,
	DW_OP_minus			= 0x1c,
	DW_OP_mod			= 0x1d,
	DW_OP_mul			= 0x1e,
	DW_OP_neg			= 0x1f,
	DW_OP_not			= 0x20,
	DW_OP_or			= 0x21,
	DW_OP_plus			= 0x22,
	DW_OP_plus_uconst		= 0x23,
	DW_OP_shl			= 0x24,
	DW_OP_shr			= 0x25,
	DW_OP_shra			= 0x26,
	DW_OP_xor			= 0x27,
	DW_OP_skip			= 0x2f,
	DW_OP_bra			= 0x28,
	DW_OP_eq			= 0x29,
	DW_OP_ge			= 0x2a,
	DW_OP_gt			= 0x2b,
	DW_OP_le			= 0x2c,
	DW_OP_lt			= 0x2d,
	DW_OP_ne			= 0x2e,
	DW_OP_lit0			= 0x30,
	DW_OP_lit1,  DW_OP_lit2,  DW_OP_lit3,  DW_OP_lit4,  DW_OP_lit5,
	DW_OP_lit6,  DW_OP_lit7,  DW_OP_lit8,  DW_OP_lit9,  DW_OP_lit10,
	DW_OP_lit11, DW_OP_lit12, DW_OP_lit13, DW_OP_lit14, DW_OP_lit15,
	DW_OP_lit16, DW_OP_lit17, DW_OP_lit18, DW_OP_lit19, DW_OP_lit20,
	DW_OP_lit21, DW_OP_lit22, DW_OP_lit23, DW_OP_lit24, DW_OP_lit25,
	DW_OP_lit26, DW_OP_lit27, DW_OP_lit28, DW_OP_lit29, DW_OP_lit30,
	DW_OP_lit31,
	DW_OP_reg0			= 0x50,
	DW_OP_reg1,  DW_OP_reg2,  DW_OP_reg3,  DW_OP_reg4,  DW_OP_reg5,
	DW_OP_reg6,  DW_OP_reg7,  DW_OP_reg8,  DW_OP_reg9,  DW_OP_reg10,
	DW_OP_reg11, DW_OP_reg12, DW_OP_reg13, DW_OP_reg14, DW_OP_reg15,
	DW_OP_reg16, DW_OP_reg17, DW_OP_reg18, DW_OP_reg19, DW_OP_reg20,
	DW_OP_reg21, DW_OP_reg22, DW_OP_reg23, DW_OP_reg24, DW_OP_reg25,
	DW_OP_reg26, DW_OP_reg27, DW_OP_reg28, DW_OP_reg29, DW_OP_reg30,
	DW_OP_reg31,
	DW_OP_breg0			= 0x70,
	DW_OP_breg1,  DW_OP_breg2,  DW_OP_breg3,  DW_OP_breg4,  DW_OP_breg5,
	DW_OP_breg6,  DW_OP_breg7,  DW_OP_breg8,  DW_OP_breg9,  DW_OP_breg10,
	DW_OP_breg11, DW_OP_breg12, DW_OP_breg13, DW_OP_breg14, DW_OP_breg15,
	DW_OP_breg16, DW_OP_breg17, DW_OP_breg18, DW_OP_breg19, DW_OP_breg20,
	DW_OP_breg21, DW_OP_breg22, DW_OP_breg23, DW_OP_breg24, DW_OP_breg25,
	DW_OP_breg26, DW_OP_breg27, DW_OP_breg28, DW_OP_breg29, DW_OP_breg30,
	DW_OP_breg31,
	DW_OP_regx			= 0x90,
	DW_OP_fbreg			= 0x91,
	DW_OP_bregx			= 0x92,
	DW_OP_piece			= 0x93,
	DW_OP_deref_size		= 0x94,
	DW_OP_xderef_size		= 0x95,
	DW_OP_nop			= 0x96,
	DW_OP_push_object_address	= 0x97,
	DW_OP_call2			= 0x98,
	DW_OP_call4			= 0x99,
	DW_OP_call_ref			= 0x9a,
	DW_OP_lo_user			= 0xe0,
	DW_OP_hi_user			= 0xff
};

static uint8_t operands[256] =
{
	[DW_OP_addr] =		OPND1 (ADDR),
	[DW_OP_const1u] =	OPND1 (VAL8),
	[DW_OP_const1s] =	OPND1 (VAL8),
	[DW_OP_const2u] =	OPND1 (VAL16),
	[DW_OP_const2s] =	OPND1 (VAL16),
	[DW_OP_const4u] =	OPND1 (VAL32),
	[DW_OP_const4s] =	OPND1 (VAL32),
	[DW_OP_const8u] =	OPND1 (VAL64),
	[DW_OP_const8s] =	OPND1 (VAL64),
	[DW_OP_pick] =		OPND1 (VAL8),
	[DW_OP_plus_uconst] =	OPND1 (ULEB128),
	[DW_OP_skip] =		OPND1 (VAL16),
	[DW_OP_bra] =		OPND1 (VAL16),
	[DW_OP_breg0 +  0] =	OPND1 (SLEB128),
	[DW_OP_breg0 +  1] =	OPND1 (SLEB128),
	[DW_OP_breg0 +  2] =	OPND1 (SLEB128),
	[DW_OP_breg0 +  3] =	OPND1 (SLEB128),
	[DW_OP_breg0 +  4] =	OPND1 (SLEB128),
	[DW_OP_breg0 +  5] =	OPND1 (SLEB128),
	[DW_OP_breg0 +  6] =	OPND1 (SLEB128),
	[DW_OP_breg0 +  7] =	OPND1 (SLEB128),
	[DW_OP_breg0 +  8] =	OPND1 (SLEB128),
	[DW_OP_breg0 +  9] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 10] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 11] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 12] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 13] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 14] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 15] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 16] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 17] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 18] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 19] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 20] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 21] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 22] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 23] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 24] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 25] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 26] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 27] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 28] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 29] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 30] =	OPND1 (SLEB128),
	[DW_OP_breg0 + 31] =	OPND1 (SLEB128),
	[DW_OP_regx] =		OPND1 (ULEB128),
	[DW_OP_fbreg] =		OPND1 (SLEB128),
	[DW_OP_bregx] =		OPND2 (ULEB128, SLEB128),
	[DW_OP_piece] =		OPND1 (ULEB128),
	[DW_OP_deref_size] =	OPND1 (VAL8),
	[DW_OP_xderef_size] =	OPND1 (VAL8),
	[DW_OP_call2] =		OPND1 (VAL16),
	[DW_OP_call4] =		OPND1 (VAL32),
	[DW_OP_call_ref] =	OPND1 (OFFSET)
};

static int fix_type(int type)
{
	if (type != ADDR)
		return type;

	switch (sizeof(unsigned long)) {
	case 4: return VAL32;
	case 8: return VAL64;
	}

	BUG_ON(1);
}

/*
 * Using macro so the READ_OPERAND can exit the main function
 * any time error is hit like DU_READ* macros.
 */
#define READ_OPERAND(addr, type, end) ({			\
	unsigned long __val = 0;				\
	u8 __type = fix_type(type);				\
								\
	switch (__type) {					\
	case VAL8:						\
		__val = (u8) DU_READ(addr, u8, addr_end);	\
		break;						\
								\
	case VAL16:						\
		__val = (u16) DU_READ(addr, u16, addr_end);	\
		break;						\
								\
	case VAL32:						\
		__val = (u32) DU_READ(addr, u32, addr_end);	\
		break;						\
								\
	case VAL64:						\
		__val = (u64) DU_READ(addr, u64, addr_end);	\
		break;						\
								\
	case ULEB128:						\
		__val = DU_READ_ULEB128(addr, addr_end);	\
		break;						\
								\
	case SLEB128:						\
		__val = DU_READ_ULEB128(addr, addr_end);	\
		break;						\
								\
	default:						\
		DU_DEBUG_EXPR("unexpected operand type %d\n",	\
			      __type);				\
		return -EINVAL;					\
	}							\
	__val;							\
})

#define MAX_EXPR_STACK_SIZE	64

#define PUSH(v)						\
do {							\
	if (sp == MAX_EXPR_STACK_SIZE) {		\
		DU_DEBUG_EXPR("stack overflow\n");	\
		return -EINVAL;				\
	}						\
	stack[sp++] = (v);				\
} while (0)

#define POP()						\
({							\
	if (!sp) {					\
		DU_DEBUG_EXPR("stack underflow\n");	\
		return -EINVAL;				\
	}						\
	stack[--sp];					\
})

# define PICK(n)					\
({							\
	unsigned int __index = sp - 1 - (n);		\
	if (__index >= MAX_EXPR_STACK_SIZE) {		\
		DU_DEBUG_EXPR("out-of-stack pick\n");	\
		return -EINVAL;				\
	}						\
	stack[__index];					\
})

#define CHK_REG(reg)						\
do {								\
	if (reg > DU_REGS_NUM) {				\
		DU_DEBUG_CFI("Invalid register number %d\n",	\
			     reg);				\
		return -EINVAL;					\
	}							\
} while (0)

#define GETREG(r) ({	\
	CHK_REG(r);	\
	regs->reg[r];	\
})

#define CHK_ADDR(addr)

bool is_big_endian(void)
{
#ifdef __LITTLE_ENDIAN
	return false;
#else
	return true;
#endif
}

/* TODO check this!!! */
#define sword(arg) arg

int du_expr(struct du_regs *regs, u8 *addr, unsigned long len,
	    unsigned long *pval)
{
	unsigned long stack[MAX_EXPR_STACK_SIZE];
	unsigned long cfa, val = 0;
	u8 *addr_end = addr + len;
	int sp = 0;

	cfa = GETREG(DU_REG_CFA_REG_COLUMN);

	DU_DEBUG_EXPR("addr %p, len=%lu, cfa=0x%lx\n",
		      addr, len, cfa);

	PUSH(cfa);

	while (addr < addr_end) {
		u8 opcode, opsign, lit, reg;
		unsigned long op1 = 0, op2 = 0;
		/*
		 * TODO check this!!!
		unsigned long op1, op2;
		*/
		unsigned long tmp1, tmp2, tmp3;

		opcode = DU_READ(addr, u8, addr_end);
		opsign = operands[opcode];

		if ((NUM_OPERANDS(opsign) > 0)) {
			op1 = READ_OPERAND(addr, OPND1_TYPE(opsign),
					   addr_end);

			if (NUM_OPERANDS(opsign) > 1)
				op2 = READ_OPERAND(addr, OPND2_TYPE(opsign),
						   addr_end);
		}

		switch (opcode) {
		case DW_OP_lit0 ... DW_OP_lit31:
			lit = opcode - DW_OP_lit0;
			PUSH(lit);

			DU_DEBUG_EXPR("OP_lit(%d)\n", lit);
			break;

		case DW_OP_breg0 ... DW_OP_breg31:
			reg = opcode - DW_OP_breg0;
			tmp1 = GETREG(reg);

			PUSH(tmp1 + op1);

			DU_DEBUG_EXPR("OP_breg(r%d,0x%lx)\n", reg, op1);
			break;

		case DW_OP_bregx:
			reg = (u8) op1;
			tmp1 = GETREG(reg);

			PUSH(tmp1 + op2);

			DU_DEBUG_EXPR("OP_bregx(r%d,0x%lx)\n", reg, op2);
			break;

		case DW_OP_reg0 ... DW_OP_reg31:
			reg = opcode - DW_OP_reg0;
			val = GETREG(reg);

			DU_DEBUG_EXPR("OP_reg(r%d)\n", reg);
			return 0;

		case DW_OP_regx:
			reg = (u8) op1;
			val = GETREG(reg);

			DU_DEBUG_EXPR("OP_regx(r%d)\n", reg);
			return 0;

		case DW_OP_addr:
		case DW_OP_const1u:
		case DW_OP_const2u:
		case DW_OP_const4u:
		case DW_OP_const8u:
		case DW_OP_constu:
		case DW_OP_const8s:
		case DW_OP_consts:
			PUSH(op1);

			DU_DEBUG_EXPR("OP_const(0x%lx)\n", op1);
			break;

		case DW_OP_const1s:
			if (op1 & 0x80)
				op1 |= ((unsigned long) -1) << 8;

			PUSH(op1);

			DU_DEBUG_EXPR("OP_const1s(%ld)\n", op1);
			break;

		case DW_OP_const2s:
			if (op1 & 0x8000)
				op1 |= ((unsigned long) -1) << 16;

			PUSH(op1);

			DU_DEBUG_EXPR("OP_const2s(%ld)\n", op1);
			break;

		case DW_OP_const4s:
			if (op1 & 0x8000)
				op1 |= (((unsigned long) -1) << 16) << 16;

			PUSH(op1);

			DU_DEBUG_EXPR("OP_const4s(%ld)\n", op1);
			break;

		case DW_OP_deref:
			tmp1 = POP();
			CHK_ADDR(tmp1);

			if (sizeof(unsigned long) == 4)
				tmp2 = DU_READ(addr, u32, addr_end);
			else
				tmp2 = DU_READ(addr, u64, addr_end);

			PUSH(tmp2);

			DU_DEBUG_EXPR("OP_deref\n");
			break;

		case DW_OP_deref_size:
			tmp1 = POP();
			CHK_ADDR(tmp1);

			switch (op1) {
			case 1:
				tmp2 = DU_READ(tmp1, u8, addr_end);
				break;

			case 2:
				tmp2 = DU_READ(tmp1, u16, addr_end);
				break;

			case 3:
			case 4:
				tmp2 = DU_READ(tmp1, u32, addr_end);

				if (op1 == 3) {
					if (is_big_endian())
						tmp2 >>= 8;
					else
						tmp2 &= 0xffffff;
				}
				break;
			case 5:
			case 6:
			case 7:
			case 8:
				tmp2 = DU_READ(tmp1, u64, addr_end);

				if (op1 != 8) {
					if (is_big_endian())
						tmp2 >>= 64 - 8 * op1;
					else
						tmp2 &= (~(unsigned long ) 0) << (8 * op1);
				}
				break;

			default:
				DU_DEBUG_EXPR("Unexpected DW_OP_deref_size size %lu\n",
					      op1);
				return -EINVAL;

			}

			PUSH(tmp2);

			DU_DEBUG_EXPR("OP_deref_size(%lu)\n", op1);
			break;

		case DW_OP_dup:
			PUSH(PICK(0));

			DU_DEBUG_EXPR("OP_dup\n");
			break;

		case DW_OP_drop:
			POP();

			DU_DEBUG_EXPR("OP_drop\n");
			break;

		case DW_OP_pick:
			PUSH(PICK(op1));

			DU_DEBUG_EXPR("OP_pick(%lu)\n", op1);
			break;

		case DW_OP_over:
			PUSH(PICK(1));

			DU_DEBUG_EXPR("OP_over\n");
			break;

		case DW_OP_swap:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(tmp1);
			PUSH(tmp2);

			DU_DEBUG_EXPR("OP_swap\n");
			break;

		case DW_OP_rot:
			tmp1 = POP();
			tmp2 = POP();
			tmp3 = POP();
			PUSH(tmp1);
			PUSH(tmp3);
			PUSH(tmp2);

			DU_DEBUG_EXPR("OP_rot\n");
			break;

		case DW_OP_abs:
			tmp1 = POP();

			if (tmp1 & ((unsigned long) 1 << (8 * sizeof(unsigned long) - 1)))
				tmp1 = -tmp1;

			PUSH(tmp1);

			DU_DEBUG_EXPR("OP_abs\n");
			break;

		case DW_OP_and:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(tmp1 & tmp2);

			DU_DEBUG_EXPR("OP_and\n");
			break;

		case DW_OP_div:
			tmp1 = POP();
			tmp2 = POP();
			if (tmp1)
				tmp1 = sword(tmp2) / sword(tmp1);
			PUSH (tmp1);

			DU_DEBUG_EXPR("OP_div\n");
			break;

		case DW_OP_minus:
			tmp1 = POP();
			tmp2 = POP();
			tmp1 = tmp2 - tmp1;
			PUSH(tmp1);

			DU_DEBUG_EXPR("OP_minus\n");
			break;

		case DW_OP_mod:
			tmp1 = POP();
			tmp2 = POP();
			if (tmp1)
				tmp1 = tmp2 % tmp1;
			PUSH(tmp1);

			DU_DEBUG_EXPR("OP_mod\n");
			break;

		case DW_OP_mul:
			tmp1 = POP();
			tmp2 = POP();
			if (tmp1)
				tmp1 = tmp2 * tmp1;
			PUSH(tmp1);

			DU_DEBUG_EXPR("OP_mul\n");
			break;

		case DW_OP_neg:
			PUSH(-POP());

			DU_DEBUG_EXPR("OP_neg\n");
			break;

		case DW_OP_not:
			PUSH(~POP());

			DU_DEBUG_EXPR("OP_not\n");
			break;

		case DW_OP_or:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(tmp1 | tmp2);

			DU_DEBUG_EXPR("OP_or\n");
			break;

		case DW_OP_plus:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(tmp1 + tmp2);

			DU_DEBUG_EXPR("OP_plus\n");
			break;

		case DW_OP_plus_uconst:
			tmp1 = POP();
			PUSH(tmp1 + op1);

			DU_DEBUG_EXPR("OP_plus_uconst(%lu)\n", op1);
			break;

		case DW_OP_shl:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(tmp2 << tmp1);

			DU_DEBUG_EXPR("OP_shl\n");
			break;

		case DW_OP_shr:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(tmp2 >> tmp1);

			DU_DEBUG_EXPR("OP_shr\n");
			break;

		case DW_OP_shra:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(sword(tmp2) >> tmp1);

			DU_DEBUG_EXPR("OP_shra\n");
			break;

		case DW_OP_xor:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(tmp1 ^ tmp2);

			DU_DEBUG_EXPR("OP_xor\n");
			break;

		case DW_OP_le:
			tmp1 = POP();
			tmp2 = POP();
			PUSH (sword(tmp1) <= sword(tmp2));

			DU_DEBUG_EXPR("OP_le\n");
			break;

		case DW_OP_ge:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(sword(tmp1) >= sword(tmp2));

			DU_DEBUG_EXPR("OP_ge\n");
			break;

		case DW_OP_eq:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(sword(tmp1) == sword(tmp2));

			DU_DEBUG_EXPR("OP_eq\n");
			break;

		case DW_OP_lt:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(sword(tmp1) < sword(tmp2));

			DU_DEBUG_EXPR("OP_lt\n");
			break;

		case DW_OP_gt:
			tmp1 = POP();
			tmp2 = POP();
			PUSH(sword(tmp1) > sword(tmp2));

			DU_DEBUG_EXPR("OP_gt\n");
			break;

		case DW_OP_ne:
			tmp1 = POP();
			tmp2 = POP();
			PUSH (sword(tmp1) != sword(tmp2));

			DU_DEBUG_EXPR("OP_ne\n");
			break;

		case DW_OP_skip:
			*addr += (int16_t) op1;

			DU_DEBUG_EXPR("OP_skip(%d)\n", (int16_t) op1);
			break;

		case DW_OP_bra:
			tmp1 = POP();
			if (tmp1)
				*addr += (int16_t) op1;

			DU_DEBUG_EXPR("OP_skip(%d)\n", (int16_t) op1);
			break;

		case DW_OP_nop:
			DU_DEBUG_EXPR("OP_nop\n");
			break;

		case DW_OP_call2:
		case DW_OP_call4:
		case DW_OP_call_ref:
		case DW_OP_fbreg:
		case DW_OP_piece:
		case DW_OP_push_object_address:
		case DW_OP_xderef:
		case DW_OP_xderef_size:
		default:
			DU_DEBUG_EXPR("Unexpected opcode 0x%x\n", opcode);
			return -EINVAL;
		} /* switch opcode */
	}

	val = POP();
	*pval = val;

	DU_DEBUG_EXPR("value = 0x%lx\n", val);
	return 0;
}
