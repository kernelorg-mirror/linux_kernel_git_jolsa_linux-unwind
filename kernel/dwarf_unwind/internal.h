#ifndef DWARF_UNWIND_INTERNAL_H
#define DWARF_UNWIND_INTERNAL_H

extern unsigned int dwarf_unwind_debug;

#define DU_DEBUG(mask, fmt, args...)				\
do {								\
	if (!dwarf_unwind_debug ||				\
	    !(dwarf_unwind_debug & DU_DEBUG_##mask))		\
		break;						\
	printk(# mask " [%s:%05d] ", __FUNCTION__, __LINE__);	\
	printk(fmt, ## args);					\
} while (0)

#endif /* DWARF_UNWIND_INTERNAL_H */
