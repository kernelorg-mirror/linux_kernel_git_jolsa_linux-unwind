#ifndef DWARF_UNWIND_INTERNAL_H
#define DWARF_UNWIND_INTERNAL_H

extern unsigned int dwarf_unwind_debug;

enum {
	DU_DEBUG_READ		= 1U << 0,
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

#endif /* DWARF_UNWIND_INTERNAL_H */
