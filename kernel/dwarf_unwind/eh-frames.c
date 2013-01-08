
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "internal.h"

#define DU_EH_FRAME_CIE 0
#define DU_EXT_LO	0xfffffff0
#define DU_EXT_HI	0xffffffff
#define DU_EXT_DWARF64	DU_EXT_HI

#define debug DU_DEBUG_EH_FRAMES

extern char __start_eh_frame[];
extern char __stop_eh_frame[];

struct parse_entry {
	u8 *start;
	u8 *end;
	int is64;
	union {
		u64 type;
		u8* cie_addr;
	};
	u8 *entries_start;
	u8 *entries_end;
};

static int valid_align(struct du_cie *cie)
{
	return ((cie->align_code < 255) &&
		(cie->align_data < 127) &&
		(cie->align_data > -127));
}

static int parse_entry_cie(struct du_frames *frames, int *ilen,
			   u8 *p, struct parse_entry *entry)
{
	u8 ver, *end = entry->end;
	struct du_cie cie = {
		.addr = entry->start,
	};
	char *aug;

	ver = DU_READ(p, u8, end);
	aug = DU_READ_STR(p, end);

	cie.align_code = DU_READ_ULEB128(p, end);
	cie.align_data = DU_READ_SLEB128(p, end);

	if (!valid_align(&cie)) {
		debug("failed: align_code %x, align_data %x\n",
		      cie.align_code, cie.align_data);
		return -EINVAL;
	}

	if (ver == 1)
		cie.ret_addr_column = DU_READ(p, u8, end);
	else
		cie.ret_addr_column = DU_READ_ULEB128(p, end);

	if (aug[0] == 'z') {
		u64 length;

		cie.aug_z = true;

		length = DU_READ_ULEB128(p, end);
		if ((p + length) >= end) {
			debug("failed: 'z' len '0x%llx' over end (%p)\n",
			      length, p);
			return -EINVAL;
		}

		aug++;
        }

	while (*aug) {
		if (*aug == 'L') {
			p += 1;
			aug++;
		} else if (*aug == 'R') {
			cie.encoding = DU_READ(p, u8, end);
			aug++;
		} else {
			debug("failed: unknow augmentation '%c'\n", *aug);
			return -EINVAL;
		}
	}

	cie.frame.icode = p;
	cie.frame.ilen  = end - p;

	*ilen += cie.frame.ilen;

	return du_cie_add(frames, &cie);
}

static int __parse_entry_fde(struct du_frames *frames, int *ilen,
			     struct du_cie *cie, u8 *p,
			     struct parse_entry *entry)
{
	struct du_fde fde = {
		.cie = cie,
	};
	u8 *end = entry->end;
	unsigned long range;

	fde.loc_start = (u8 *) DU_READ_ENCODED_VALUE(p, end, cie->encoding);
	range = DU_READ_ENCODED_VALUE(p, end, cie->encoding & 0x0f);

	if (cie->aug_z)
		DU_READ_ULEB128(p, end);

	fde.frame.icode = p;
	fde.frame.ilen  = end - p;

	fde.loc_end = (u8 *) (fde.loc_start + range);

	*ilen += fde.frame.ilen;

	return du_fde_add(frames, &fde);
}

static int parse_entry_fde(struct du_frames *frames, int *ilen,
			   u8 *p, struct parse_entry *entry)
{
	struct du_cie *cie;

        cie = du_cie_lookup(frames, entry->cie_addr);
	if (!cie)
		return -EINVAL;

	return __parse_entry_fde(frames, ilen, cie, p, entry);
}

static int parse_entry_len(u8 **p, struct parse_entry *entry)
{
	u8 *cur = *p;
	u8 *entry_end;
	u32 len32;
	u64 len;
	int is64;

	len32 = DU_READ(cur, u32, entry->entries_end);

	if (len32 >= DU_EXT_LO && len32 <= DU_EXT_HI) {
		if (len32 != DU_EXT_DWARF64)
			return -EINVAL;

		is64 = 1;
		len  = DU_READ(cur, u64, entry->entries_end);
        } else {
		is64 = 0;
		len  = len32;
	}

	entry_end = cur + len;
	if (entry_end > entry->entries_end)
		return -EINVAL;

	entry->end  = entry_end;
	entry->is64 = is64;

	*p = cur;
	return 0;
}

static int parse_entry_type(u8 **p, struct parse_entry *entry)
{
	u8 *cur = *p;
	u64 val;

	if (entry->is64)
		val = DU_READ(cur, u64, entry->end);
	else
		val = (u64) DU_READ(cur, u32, entry->end);

	if (val != DU_EH_FRAME_CIE) {
		u8 *cie_addr = cur - val - (entry->is64 ? 8 : 4);
		if (cie_addr < entry->entries_start)
			return -EINVAL;

		entry->cie_addr = cie_addr;
	} else
		entry->type = val;

	*p = cur;
	return 0;
}

static int parse_entry_is_cie(struct parse_entry *entry)
{
	return (entry->type == DU_EH_FRAME_CIE);
}

static int parse_entry(struct du_frames *frames, int *ilen,
		       u8 *p, struct parse_entry *entry)
{
	if (parse_entry_len(&p , entry))
		return -EINVAL;

	if (parse_entry_type(&p, entry))
		return -EINVAL;

	if (parse_entry_is_cie(entry))
		return parse_entry_cie(frames, ilen, p, entry);
	else
		return parse_entry_fde(frames, ilen, p, entry);
}

static int parse_limits(struct du_frames *frames, int *ilen,
			u8 *start, u8 *end)
{
	u8 *pe = start;

	while (pe < end) {
		struct parse_entry entry = {
			.start         = pe,
			.entries_start = start,
			.entries_end   = end,
		};
		int ret;

		ret = parse_entry(frames, ilen, pe, &entry);
		if (ret)
			return ret;

		pe = entry.end;
	}

	return 0;
}

static int parse(struct du_frames *frames, int *ilen)
{
	struct module *mod = frames->mod;
	u8 *start, *end;

	if (mod) {
		start = mod->ehframe_start;
		end   = mod->ehframe_stop;
	} else {
		start = __start_eh_frame;
		end   = __stop_eh_frame;
	}

	return parse_limits(frames, ilen, start, end);
}

int frames_instr(struct rb_root *rb_root, u8 **instr)
{
	struct rb_node *next = rb_first(rb_root);
	int ret = 0;

	while (next) {
		struct du_frame *pos = rb_entry(next, struct du_frame, rb_node);

		memcpy(*instr, pos->icode, pos->ilen);
		pos->icode = *instr;
		*instr += pos->ilen;

		next = rb_next(&pos->rb_node);
	}

	return ret;
}

static int init_instructions(struct du_frames *frames, int ilen)
{
	u8 *instr;

	instr = kzalloc(ilen, GFP_KERNEL);
	if (!instr)
		return -ENOMEM;

	frames_instr(&frames->rb_root_cie, &instr);
	frames_instr(&frames->rb_root_fde, &instr);

	frames->instr = instr;

	return 0;
}

static void frames_release(struct du_frames *frames)
{
	kfree(frames->instr);
}

int du_ehframe_init(struct du_frames *frames)
{
	int ilen = 0;
	int ret;

	ret = parse(frames, &ilen);
	if (!ret)
		ret = init_instructions(frames, ilen);

	return ret;
}

void du_ehframe_release(struct du_frames *frames)
{
	frames_release(frames);
}
