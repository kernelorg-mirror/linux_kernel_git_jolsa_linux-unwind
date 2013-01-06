#include <asm/dwarf_unwind_regs.h>

#define GET(i, r) dr->reg[ DU_REG_ ## i ] = pr->r
#define GET_0(i)  dr->reg[ DU_REG_ ## i ] = 0

#define SET(i, r) pr->r = dr->reg[ DU_REG_ ## i ]
#define SET_0(r)  pr->r = 0

#ifdef __i386__
void du_arch_regs_get(struct du_regs *dr, struct pt_regs *pr)
{
	GEG(X86_EAX, ax);
	GEG(X86_ECX, cx);
	GEG(X86_EDX, dx);
	GEG(X86_EBX, bx);
	GEG(X86_ESP, sp);
	GEG(X86_EBP, bp);
	GEG(X86_ESI, si);
	GEG(X86_EDI, di);
	GEG(X86_EIP, ip);
	GEG(X86_EFLAGS, flags);

	GEG_0(X86_TRAPNO);
	GEG_0(X86_ST0);
	GEG_0(X86_ST1);
	GEG_0(X86_ST2);
	GEG_0(X86_ST3);
	GEG_0(X86_ST4);
	GEG_0(X86_ST5);
	GEG_0(X86_ST6);
	GEG_0(X86_ST7);

	GET_0(CFA_REG_COLUMN);
	GET_0(CFA_OFF_COLUMN);
}

void du_arch_regs_set(struct du_regs *dr, struct pt_regs *pr)
{
	SET(X86_EAX, ax);
	SET(X86_ECX, cx);
	SET(X86_EDX, dx);
	SET(X86_EBX, bx);
	SET(X86_ESP, sp);
	SET(X86_EBP, bp);
	SET(X86_ESI, si);
	SET(X86_EDI, di);
	SET(X86_EIP, ip);
	SET(X86_EFLAGS, flags);
	SET_0(ds);
	SET_0(es);
	SET_0(fs);
	SET_0(gs);
	SET_0(ss);
	SET_0(cs);
	SET_0(orig_ax);
}
#else
void du_arch_regs_get(struct du_regs *dr, struct pt_regs *pr)
{
	GET(X86_64_RAX, ax);
	GET(X86_64_RDX, dx);
	GET(X86_64_RCX, cx);
	GET(X86_64_RBX, bx);
	GET(X86_64_RSI, si);
	GET(X86_64_RDI, di);
	GET(X86_64_RBP, bp);
	GET(X86_64_RSP, sp);
	GET(X86_64_R8,  r8);
	GET(X86_64_R9,  r9);
	GET(X86_64_R10, r10);
	GET(X86_64_R11, r11);
	GET(X86_64_R12, r12);
	GET(X86_64_R13, r13);
	GET(X86_64_R14, r14);
	GET(X86_64_R15, r15);
	GET(X86_64_RIP, ip);

	GET_0(CFA_REG_COLUMN);
	GET_0(CFA_OFF_COLUMN);
}

void du_arch_regs_set(struct du_regs *dr, struct pt_regs *pr)
{
	SET(X86_64_RAX, ax);
	SET(X86_64_RDX, dx);
	SET(X86_64_RCX, cx);
	SET(X86_64_RBX, bx);
	SET(X86_64_RSI, si);
	SET(X86_64_RDI, di);
	SET(X86_64_RBP, bp);
	SET(X86_64_RSP, sp);
	SET(X86_64_R8,  r8);
	SET(X86_64_R9,  r9);
	SET(X86_64_R10, r10);
	SET(X86_64_R11, r11);
	SET(X86_64_R12, r12);
	SET(X86_64_R13, r13);
	SET(X86_64_R14, r14);
	SET(X86_64_R15, r15);
	SET(X86_64_RIP, ip);

	SET_0(orig_ax);
	SET_0(cs);
	SET_0(ss);
}
#endif /* __i386__ */
