#include <linux/linkage.h>
#include <asm/ptrace-abi.h>

.text
ENTRY(regs_load)
	movl %ebx, EBX*4(%rdi)
	movl %ecx, ECX*4(%rdi)
	movl %edx, EDX*4(%rdi)
	movl %esi, ESI*4(%rdi)
	movl %edi, EDI*4(%rdi)
	movl %ebp, EBP*4(%rdi)
	movl %eax, EAX*4(%rdi)

	movl %ds, DS*4(%rdi)
	movl %es, ES*4(%rdi)
	movl %fs, FS*4(%rdi)
	movl %gs, GS*4(%rdi)
	movl %cs, CS*4(%rdi)
	movl %ss, SS*4(%rdi)

	pushf
	popl %eax
	movl %eax, EFL*4(%rdi)

	movl $0, ORIG_EAX*4(%rdi)

	leal 4(%esp), %eax /* exclude this call.  */
	movl %eax, UESP*4(%rdi)
	movl 0(%esp), %eax
	movl %eax, EIP(%edi)

	ret
END(regs_load)
