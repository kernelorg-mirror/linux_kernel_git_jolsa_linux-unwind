#ifndef _ASM_X86_DWARF_UNWIND_REGS_H
#define _ASM_X86_DWARF_UNWIND_REGS_H

#include <linux/ptrace.h>

#ifdef __i386__
enum du_arch_regs {
	/* Standard x86 registers. */
	DU_REG_X86_EAX,
	DU_REG_X86_ECX,
	DU_REG_X86_EDX,
	DU_REG_X86_EBX,
	DU_REG_X86_ESP,
	DU_REG_X86_EBP,
	DU_REG_X86_ESI,
	DU_REG_X86_EDI,
	DU_REG_X86_EIP,
	DU_REG_X86_EFLAGS,
	DU_REG_X86_TRAPNO,
	DU_REG_X86_ST0,
	DU_REG_X86_ST1,
	DU_REG_X86_ST2,
	DU_REG_X86_ST3,
	DU_REG_X86_ST4,
	DU_REG_X86_ST5,
	DU_REG_X86_ST6,
	DU_REG_X86_ST7

	/* Trating CFA as special register. */
	DU_REG_CFA_REG_COLUMN,
	DU_REG_CFA_OFF_COLUMN,

	DU_REGS_NUM,

	DU_REG_SP  = DU_REG_X86_ESP,
	DU_REG_IP  = DU_REG_X86_EIP,
	DU_REG_CFA = DU_REG_CFA_REG_COLUMN,
};
#else
enum du_arch_regs {
	/* Standard x86_64 registers. */
	DU_REG_X86_64_RAX,
	DU_REG_X86_64_RDX,
	DU_REG_X86_64_RCX,
	DU_REG_X86_64_RBX,
	DU_REG_X86_64_RSI,
	DU_REG_X86_64_RDI,
	DU_REG_X86_64_RBP,
	DU_REG_X86_64_RSP,
	DU_REG_X86_64_R8,
	DU_REG_X86_64_R9,
	DU_REG_X86_64_R10,
	DU_REG_X86_64_R11,
	DU_REG_X86_64_R12,
	DU_REG_X86_64_R13,
	DU_REG_X86_64_R14,
	DU_REG_X86_64_R15,
	DU_REG_X86_64_RIP,

	/* Trating CFA as special register. */
	DU_REG_CFA_REG_COLUMN,
	DU_REG_CFA_OFF_COLUMN,

	DU_REGS_NUM,

	DU_REG_SP  = DU_REG_X86_64_RSP,
	DU_REG_IP  = DU_REG_X86_64_RIP,
	DU_REG_CFA = DU_REG_CFA_REG_COLUMN,
};
#endif /* __i386__ */

struct du_regs {
	unsigned long reg[DU_REGS_NUM];
};

void du_arch_regs_get(struct du_regs *dr, struct pt_regs *pr);
void du_arch_regs_set(struct du_regs *dr, struct pt_regs *pr);

#endif /* _ASM_X86_DWARF_UNWIND_REGS_H */
