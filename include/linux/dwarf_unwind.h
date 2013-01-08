#ifndef DWARF_UNWIND_H
#define DWARF_UNWIND_H

#include <linux/ptrace.h>

typedef int (*dwarf_unwind_cb)(struct pt_regs *regs, void *data);

int dwarf_unwind(struct pt_regs *regs, dwarf_unwind_cb cb, void *data);

#endif /* DWARF_UNWIND_H */
