#ifndef DWARF_UNWIND_INTERNAL_H
#define DWARF_UNWIND_INTERNAL_H

#include <linux/slab.h>
#include <linux/rbtree.h>
#include <asm/dwarf_unwind_regs.h>

struct du_frames {
	struct module           *mod;
	struct list_head        list;

	struct kmem_cache       *kmem_cie;
	struct kmem_cache       *kmem_fde;

	struct rb_root          rb_root_cie;
	struct rb_root          rb_root_fde;

	atomic_t		refcount;

#ifdef CONFIG_DWARF_UNWIND_EH_FRAMES
	u8 *instr;
#endif

#ifdef CONFIG_DWARF_UNWIND_DEBUGFS
	struct dentry *d_mod;
	struct dentry *d_cie;
	struct dentry *d_fde;
#endif
};

struct du_frame {
	struct rb_node rb_node;

	u16      ilen;
	u8      *icode;

#ifdef CONFIG_DWARF_UNWIND_DEBUGFS
	unsigned long	 offset;
#endif
};

struct du_cie {
	struct du_frame frame;

	u8	*addr;
	u8	 encoding;
	u8	 ret_addr_column;
	u8	 align_code;
	s8	 align_data;

	bool	 aug_z;
};

struct du_fde {
	struct du_frame frame;

	struct du_cie 	*cie;
	u8		*loc_start;
	u8		*loc_end;
};

int du_cie_add(struct du_frames *frames, struct du_cie *cie);
int du_fde_add(struct du_frames *frames, struct du_fde *fde);

enum du_location {
	DU_LOCATION_SAME = 0,
	DU_LOCATION_UNDEF,
	DU_LOCATION_REG,
	DU_LOCATION_MEMORY,
	DU_LOCATION_VALUE,
	DU_LOCATION_EXPR,
	DU_LOCATION_EXPR_VALUE,
};

struct du_state_reg {
	enum du_location loc;
	union {
		unsigned long val;
		struct {
			u8 *expr;
			unsigned long len;
		};
	};
};

struct du_state_regs {
	struct du_state_reg reg[DU_REGS_NUM];
};

#define DWARF_UNWIND_CFA_STACK_MAX 5

struct du_state {
	struct du_state_regs state_initial;
	struct du_state_regs state_current[DWARF_UNWIND_CFA_STACK_MAX];
	unsigned cur;
};

int du_cfi(struct du_fde *fde, struct du_state *state,
	       unsigned long ip, struct du_frame *frame);

struct du_fde *du_fde_lookup(struct du_frames *frames, unsigned long addr);
struct du_cie* du_cie_lookup(struct du_frames *frames, u8 *addr);
void du_frames_put(struct du_frames *frames);
struct du_frames* du_frames_find(struct module *mod);

extern unsigned int dwarf_unwind_debug;

enum {
	DU_DEBUG_READ		= 1U << 0,
	DU_DEBUG_FRAMES		= 1U << 1,
	DU_DEBUG_EH_FRAMES	= 1U << 2,
	DU_DEBUG_CFI		= 1U << 3,
};

#define DU_DEBUG(mask, fmt, args...)				\
do {								\
	if (!dwarf_unwind_debug ||				\
	    !(dwarf_unwind_debug & DU_DEBUG_##mask))		\
		break;						\
	printk(# mask " [%s:%05d] ", __FUNCTION__, __LINE__);	\
	printk(fmt, ## args);					\
} while (0)

#define DU_DEBUG_READ(fmt, args...)		DU_DEBUG(READ, fmt, ## args)
#define DU_DEBUG_FRAMES(fmt, args...)		DU_DEBUG(FRAMES, fmt, ## args)
#define DU_DEBUG_EH_FRAMES(fmt, args...)	DU_DEBUG(EH_FRAMES, fmt, ## args)
#define DU_DEBUG_CFI(fmt, args...)		DU_DEBUG(CFI, fmt, ## args)

int du_read_uleb128(u8 **p, u8 *end, u64 *val);
int du_read_sleb128(u8 **p, u8 *end, s64 *val);
int du_read_encoded_value(u8 **p, u8 *end, unsigned long *val, u8 encoding);
char *du_read_str(u8 **p, u8 *end);

#define DU_READ(ptr, type, end) ({				\
	type *__p = (type *) ptr;				\
	type  __v;						\
	if ((__p + 1) > (type *) end) {				\
		DU_DEBUG_READ("FAILED du_read %p\n", __p + 1);	\
		return -EINVAL;					\
	}							\
	__v = *__p++;						\
	ptr = (typeof(ptr)) __p;				\
	__v;							\
	})

#define DU_READ_ULEB128(ptr, end) ({					\
	u64 __v;							\
	if (du_read_uleb128(&ptr, end, &__v))	{			\
		DU_DEBUG_READ("FAILED du_read_uleb128 %p\n", ptr);	\
		return -EINVAL;						\
	}								\
	__v;								\
	})

#define DU_READ_SLEB128(ptr, end) ({					\
	s64 __v;							\
	if (du_read_sleb128(&ptr, end, &__v)) {				\
		DU_DEBUG_READ("FAILED du_read_sleb128 %p\n", ptr);	\
		return -EINVAL;						\
	}								\
	__v;								\
	})

#define DU_READ_ENCODED_VALUE(ptr, end, enc) ({				\
	unsigned long __v;						\
	if (du_read_encoded_value(&ptr, end, &__v, enc)) {		\
		DU_DEBUG_READ("FAILED du_read_encoded_value %p\n", ptr);\
		return -EINVAL;						\
	}								\
	__v;								\
	})

#define DU_READ_STR(p, end) ({						\
	char *__c = du_read_str(&p, end);				\
	if (!__c) {							\
		DU_DEBUG_READ("FAILED du_read_str %p\n", p);		\
		return -EINVAL;						\
	}								\
	__c;								\
	})

#ifdef CONFIG_DWARF_UNWIND_EH_FRAMES
int  du_ehframe_init(struct du_frames *frames);
void du_ehframe_release(struct du_frames *frames);
#endif /* CONFIG_DWARF_UNWIND_EH_FRAMES */

#endif /* DWARF_UNWIND_INTERNAL_H */
