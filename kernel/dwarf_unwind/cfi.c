
#include "internal.h"

#define DWARF_CFA_OPCODE_MASK	0xc0
#define DWARF_CFA_OPERAND_MASK	0x3f

enum {
	DW_CFA_advance_loc			= 0x40,
	DW_CFA_offset				= 0x80,
	DW_CFA_restore				= 0xc0,
	DW_CFA_nop				= 0x00,
	DW_CFA_set_loc				= 0x01,
	DW_CFA_advance_loc1			= 0x02,
	DW_CFA_advance_loc2			= 0x03,
	DW_CFA_advance_loc4			= 0x04,
	DW_CFA_offset_extended			= 0x05,
	DW_CFA_restore_extended			= 0x06,
	DW_CFA_undefined			= 0x07,
	DW_CFA_same_value			= 0x08,
	DW_CFA_register				= 0x09,
	DW_CFA_remember_state			= 0x0a,
	DW_CFA_restore_state			= 0x0b,
	DW_CFA_def_cfa				= 0x0c,
	DW_CFA_def_cfa_register			= 0x0d,
	DW_CFA_def_cfa_offset			= 0x0e,
	DW_CFA_def_cfa_expression		= 0x0f,
	DW_CFA_expression			= 0x10,
	DW_CFA_offset_extended_sf		= 0x11,
	DW_CFA_def_cfa_sf			= 0x12,
	DW_CFA_def_cfa_offset_sf		= 0x13,
	DW_CFA_val_expression			= 0x16,
	DW_CFA_lo_user				= 0x1c,
	DW_CFA_MIPS_advance_loc8		= 0x1d,
	DW_CFA_GNU_window_save			= 0x2d,
	DW_CFA_GNU_args_size			= 0x2e,
	DW_CFA_GNU_negative_offset_extended	= 0x2f,
	DW_CFA_hi_user				= 0x3c,
};

static void setreg(struct du_state_regs *rs, unsigned long reg,
		   enum du_location loc, unsigned long val)
{
	rs->reg[reg].loc = loc;
	rs->reg[reg].val = val;
}

static void setreg_expr(struct du_state_regs *rs, unsigned long reg,
			enum du_location loc, u8 *addr, unsigned long len)
{
	rs->reg[reg].loc  = DU_LOCATION_EXPR;
	rs->reg[reg].expr = addr;
	rs->reg[reg].len  = len;
}

#define STATE_CURRENT (&state->state_current[state->cur])
#define STATE_INITIAL (&state->state_initial)

#define CHK_REG(reg)						\
do {								\
	if (reg > DU_REGS_NUM) {				\
		DU_DEBUG_CFI("Invalid register number %d\n",	\
			     reg);				\
		return -EINVAL;					\
	}							\
} while (0)

#define SETREG(reg, loc, val)			\
do {						\
	CHK_REG(reg);				\
	setreg(STATE_CURRENT, reg, loc, val);	\
} while (0)					\

#define SETREG_EXPR(reg, loc, addr, len)	\
do {						\
	CHK_REG(reg);				\
	setreg_expr(STATE_CURRENT, reg, loc, addr, len);\
} while (0)					\

int du_cfi(struct du_fde *fde, struct du_state *state,
	   unsigned long ip, struct du_frame *frame)
{
	struct du_cie *cie = fde->cie;
	u8 *addr = frame->icode;
	u8 *addr_end = frame->icode + frame->ilen;
	u8 *curr_ip = fde->loc_start;

	while ((curr_ip <= (u8 *) ip) && (addr < addr_end)) {
		u8 op, operand, reg, val8;
		unsigned long val, len;
		u16 val16;
		u32 val32;

		op = DU_READ(addr, u8, addr_end);

		/* TODO check operand */
		operand = (u8) -1;

		if (op & DWARF_CFA_OPCODE_MASK) {
			operand = op & DWARF_CFA_OPERAND_MASK;
			op &= ~DWARF_CFA_OPERAND_MASK;
		}

		switch (op) {
		case DW_CFA_advance_loc:
			curr_ip += operand * cie->align_code;
			DU_DEBUG_CFI("CFA_advance_loc to %p\n", curr_ip);
			break;

		case DW_CFA_advance_loc1:
			val8 = DU_READ(addr, u8, addr_end);
			curr_ip += val8 * cie->align_code;
			DU_DEBUG_CFI("CFA_advance_loc1 to %p\n", curr_ip);
			break;

		case DW_CFA_advance_loc2:
			val16 = DU_READ(addr, u16, addr_end);
			curr_ip += val16 * cie->align_code;
			DU_DEBUG_CFI("CFA_advance_loc2 to %p\n", curr_ip);
			break;

		case DW_CFA_advance_loc4:
			val32 = DU_READ(addr, u32, addr_end);
			curr_ip += val32 * cie->align_code;
			DU_DEBUG_CFI("CFA_advance_loc4 to %p\n", curr_ip);
			break;

		case DW_CFA_MIPS_advance_loc8:
			DU_DEBUG_CFI("FAILED DW_CFA_MIPS_advance_loc8\n");
			return -EINVAL;

		case DW_CFA_offset:
			val = DU_READ_ULEB128(addr, addr_end);
			val *= cie->align_data;

			SETREG(operand, DU_LOCATION_MEMORY, val);

			DU_DEBUG_CFI("CFA_offset r%u at cfa+%lu\n",
				     operand, val);
			break;

		case DW_CFA_offset_extended:
			reg = DU_READ_ULEB128(addr, addr_end);
			val = DU_READ_ULEB128(addr, addr_end);
			val *= cie->align_data;

			SETREG(reg, DU_LOCATION_MEMORY, val);

			DU_DEBUG_CFI("CFA_offset_extended r%u at cf+0x%lx\n",
				     reg, val);
			break;

		case DW_CFA_offset_extended_sf:
			reg = DU_READ_ULEB128(addr, addr_end);
			val = DU_READ_SLEB128(addr, addr_end);
			val *= cie->align_data;

			SETREG(reg, DU_LOCATION_MEMORY, val);

			DU_DEBUG_CFI("DW_CFA_offset_extended_sf r%u at cf+0x%lx\n",
				     reg, val);
			break;

		case DW_CFA_restore:
			CHK_REG(operand);
			STATE_CURRENT->reg[operand] = STATE_INITIAL->reg[operand];

			DU_DEBUG_CFI("CFA_restore r%u\n", operand);
			break;

		case DW_CFA_restore_extended:
			reg = DU_READ_ULEB128(addr, addr_end);
			CHK_REG(operand);

			STATE_CURRENT->reg[reg] = STATE_INITIAL->reg[reg];
			DU_DEBUG_CFI("CFA_restore_extended r%u\n", reg);
			break;

		case DW_CFA_nop:
			DU_DEBUG_CFI("DW_CFA_nop\n");
			break;

		case DW_CFA_set_loc:
			curr_ip = (u8 *) DU_READ_ENCODED_VALUE(addr, addr_end,
							       cie->encoding);

			DU_DEBUG_CFI("CFA_set_loc to %p\n", curr_ip);
			break;

		case DW_CFA_undefined:
			reg = DU_READ_ULEB128(addr, addr_end);
			SETREG(reg, DU_LOCATION_UNDEF, 0);

			DU_DEBUG_CFI("CFA_undefined r%u\n", reg);
			break;

		case DW_CFA_same_value:
			reg = DU_READ_ULEB128(addr, addr_end);
			SETREG(reg, DU_LOCATION_SAME, 0);

			DU_DEBUG_CFI("CFA_same_value r%u\n", reg);
			break;

		case DW_CFA_register:
			reg = DU_READ_ULEB128(addr, addr_end);
			val = DU_READ_ULEB128(addr, addr_end);
			SETREG(reg, DU_LOCATION_REG, val);

			DU_DEBUG_CFI("CFA_register r%u to r%lu\n", reg, val);
			break;

		case DW_CFA_remember_state:
			if ((state->cur + 1) >= DWARF_UNWIND_CFA_STACK_MAX) {
				DU_DEBUG_CFI("FAILED stack top reached\n");
				return -EINVAL;
			}

			DU_DEBUG_CFI("CFA_remember_state %d\n", state->cur);
			state->cur++;
			break;

		case DW_CFA_restore_state:
			if (!state->cur) {
				DU_DEBUG_CFI("FAILED stack underflow\n");
				return -EINVAL;
			}

			state->cur--;
			DU_DEBUG_CFI("DW_CFA_restore_state %d\n", state->cur);
			break;

		case DW_CFA_def_cfa:
			reg = DU_READ_ULEB128(addr, addr_end);
			val = DU_READ_ULEB128(addr, addr_end);

			SETREG(DU_REG_CFA_REG_COLUMN,
			       DU_LOCATION_REG, reg);
			SETREG(DU_REG_CFA_OFF_COLUMN,
			       DU_LOCATION_VALUE, val);

			DU_DEBUG_CFI("CFA_def_cfa r%u+0x%lx\n", reg, val);
			break;

		case DW_CFA_def_cfa_sf:
			reg = DU_READ_ULEB128(addr, addr_end);
			val = DU_READ_SLEB128(addr, addr_end);
			val *= cie->align_data;

			SETREG(DU_REG_CFA_REG_COLUMN,
			       DU_LOCATION_REG, reg);
			SETREG(DU_REG_CFA_OFF_COLUMN,
			       DU_LOCATION_VALUE, val);

			DU_DEBUG_CFI("CFA_def_cfa_sf r%u+0x%lx\n", reg, val);
			break;

		case DW_CFA_def_cfa_register:
			reg = DU_READ_ULEB128(addr, addr_end);

			SETREG(DU_REG_CFA_REG_COLUMN,
			       DU_LOCATION_REG, reg);

			DU_DEBUG_CFI("CFA_def_cfa_register r%u\n", reg);
			break;

		case DW_CFA_def_cfa_offset:
			val = DU_READ_ULEB128(addr, addr_end);

			SETREG(DU_REG_CFA_OFF_COLUMN,
			       DU_LOCATION_VALUE, val);

			DU_DEBUG_CFI("CFA_def_cfa_offset 0x%lx\n", val);
			break;

		case DW_CFA_def_cfa_offset_sf:
			val = DU_READ_SLEB128(addr, addr_end);
			val *= cie->align_data;

			SETREG(DU_REG_CFA_OFF_COLUMN,
			       DU_LOCATION_VALUE, val);

			DU_DEBUG_CFI("CFA_def_cfa_offset_sf 0x%lx\n", val);
			break;

		case DW_CFA_def_cfa_expression:
			len = DU_READ_ULEB128(addr, addr_end);

			SETREG_EXPR(DU_REG_CFA_REG_COLUMN,
				    DU_LOCATION_EXPR,
				    addr, len);

			addr += len;
			DU_DEBUG_CFI("CFA_def_cfa_expr @ %p [%lu bytes]\n",
				     addr, len);
			break;

		case DW_CFA_expression:
			reg = DU_READ_ULEB128(addr, addr_end);
			len = DU_READ_ULEB128(addr, addr_end);

			SETREG_EXPR(DU_REG_CFA_REG_COLUMN,
				    DU_LOCATION_EXPR,
				    addr, len);

			addr += len;

			DU_DEBUG_CFI("CFA_expression r%u @ %p [%lu bytes]\n",
				     reg, addr, len);
			break;

		case DW_CFA_val_expression:
			reg = DU_READ_ULEB128(addr, addr_end);
			len = DU_READ_ULEB128(addr, addr_end);

			SETREG_EXPR(DU_REG_CFA_REG_COLUMN,
				    DU_LOCATION_EXPR_VALUE,
				    addr, len);

			addr += len;

			DU_DEBUG_CFI("CFA_expression r%u @ %p [%lu bytes]\n",
				     reg, addr, len);
			break;

		case DW_CFA_GNU_negative_offset_extended:
			reg = DU_READ_ULEB128(addr, addr_end);
			val = DU_READ_ULEB128(addr, addr_end);
			val *= -cie->align_data;

			SETREG(reg, DU_LOCATION_MEMORY, val);

			DU_DEBUG_CFI("CFA_GNU_negative_offset_extended cfa+0x%lx\n", val);
			break;

		case DW_CFA_GNU_window_save:
			/*
			 * This is a special CFA to handle all 16 windowed
			 * registers on SPARC.
			 */

		default:
			DU_DEBUG_CFI("FAILED unexpected CFA op 0x%x\n", op);
			return -EINVAL;
		}
	}

	return 0;
}
