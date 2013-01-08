
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/errno.h>
#include "internal.h"

int du_read_uleb128(u8 **p, u8 *end, u64 *val)
{
	int shift = 0;
	u8 byte, *cur = *p;

	*val = 0;
	do {
		if ((cur >= end) || (shift > sizeof(u64) * 8))
			return -EINVAL;

		byte = *cur++;
		*val |= ((u64) byte & 0x7f) << shift;
		shift += 7;
	} while (byte & 0x80);

	*p = cur;
	return 0;
}

int du_read_sleb128(u8 **p, u8 *end, s64 *val)
{
	int shift = 0;
	u8 byte, *cur = *p;

	*val = 0;
	do {
		if ((cur >= end) || (shift > sizeof(s64) * 8))
			return -EINVAL;

		byte = *cur++;
		*val |= ((u64) byte & 0x7f) << shift;
		shift += 7;
	} while (byte & 0x80);

	if (shift < 8 * sizeof(*val) && (byte & 0x40) != 0)
		*val |= ((s64) -1) << shift;

	*p = cur;
	return 0;
}

char *du_read_str(u8 **p, u8 *end)
{
	u8 *cur = *p;
	size_t max = (u8 *) end - cur, len;

	len = strnlen(cur, max);
	if (len == max)
		return NULL;

	*p = cur + len + 1;
	return cur;
};

#define DW_EH_PE_FORMAT_MASK   0x0f
#define DW_EH_PE_APPL_MASK     0x70
#define DW_EH_PE_ptr           0x00
#define DW_EH_PE_omit          0xff
#define DW_EH_PE_absptr        0x00
#define DW_EH_PE_omit          0xff
#define DW_EH_PE_udata4        0x03
#define DW_EH_PE_udata8        0x04
#define DW_EH_PE_sdata4        0x0b
#define DW_EH_PE_sdata8        0x0c
#define DW_EH_PE_pcrel         0x10

int du_read_encoded_value(u8 **p, u8 *end, unsigned long *val,
				   u8 encoding)
{
	u8 *cur = *p;
	*val = 0;

	switch (encoding) {
	case DW_EH_PE_ptr:
		*val = DU_READ(cur, unsigned long, end);
		goto out;
	}

	switch (encoding & DW_EH_PE_APPL_MASK) {
	case DW_EH_PE_absptr:
		break;
	case DW_EH_PE_pcrel:
		*val = (unsigned long) cur;
		break;
	default:
		return -EINVAL;
	}

	if ((encoding & 0x07) == 0x00)
		encoding |= DW_EH_PE_udata4;

	switch (encoding & DW_EH_PE_FORMAT_MASK) {
	case DW_EH_PE_sdata4:
		*val += DU_READ(cur, s32, end);
		break;
	case DW_EH_PE_udata4:
		*val += DU_READ(cur, u32, end);
		break;
	case DW_EH_PE_sdata8:
		*val += DU_READ(cur, s64, end);
		break;
	case DW_EH_PE_udata8:
		*val += DU_READ(cur, u64, end);
		break;
	default:
		return -EINVAL;
	}

 out:
	*p = cur;
	return 0;
}
