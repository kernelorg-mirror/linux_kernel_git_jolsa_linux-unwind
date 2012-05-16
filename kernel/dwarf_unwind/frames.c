#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/hash.h>
#include <linux/debugfs.h>
#include "internal.h"

static LIST_HEAD(frames_list);
static DEFINE_SPINLOCK(frames_lock);

static void frames_free(struct du_frames *frames);

static void frames_get(struct du_frames *frames)
{
	WARN_ON(!atomic_inc_not_zero(&frames->refcount));
}

static struct du_frames* frames_alloc(struct module *mod)
{
	struct du_frames *frames;

	frames = kzalloc(sizeof(*frames), GFP_KERNEL);
	if (!frames)
		return NULL;

	frames->mod = mod;

	frames->rb_root_cie = RB_ROOT;
	frames->rb_root_fde = RB_ROOT;

	INIT_LIST_HEAD(&frames->list);
	atomic_set(&frames->refcount, 1);
	return frames;
}

static struct du_frames* frames_find(struct module *mod)
{
	struct du_frames *frames;

	list_for_each_entry(frames, &frames_list, list)
		if (frames->mod == mod)
			return frames;

	return NULL;
}

struct du_frames* du_frames_find(struct module *mod)
{
	struct du_frames *frames;
	unsigned long flags;

	spin_lock_irqsave(&frames_lock, flags);
	frames = frames_find(mod);
	if (frames)
		frames_get(frames);
	spin_unlock_irqrestore(&frames_lock, flags);

	DU_DEBUG_FRAMES("mod %p, frames %p\n", mod, frames);
	return frames;
}

void du_frames_put(struct du_frames *frames)
{
	if (atomic_dec_and_test(&frames->refcount))
		frames_free(frames);
}

typedef int (frame_add_cb)(struct du_frame *a, struct du_frame *b);

static int cie_cmp(struct du_frame *a, struct du_frame *b)
{
	struct du_cie *cie_a = container_of(a, struct du_cie, frame);
	struct du_cie *cie_b = container_of(b, struct du_cie, frame);

	return cie_a->addr - cie_b->addr;
}

static int fde_cmp(struct du_frame *a, struct du_frame *b)
{
	struct du_fde *fde_a = container_of(a, struct du_fde, frame);
	struct du_fde *fde_b = container_of(b, struct du_fde, frame);

	return fde_a->loc_start - fde_b->loc_start;
}

static int frame_add(struct rb_root *root, struct du_frame *frame,
		     frame_add_cb cmp)
{
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;

	while (*p != NULL) {
		struct du_frame *f;

		parent = *p;
		f = rb_entry(parent, struct du_frame, rb_node);

		if (cmp(frame, f) < 0)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	rb_link_node(&frame->rb_node, parent, p);
	rb_insert_color(&frame->rb_node, root);
	return 0;
}

int du_cie_add(struct du_frames *frames, struct du_cie *templ)
{
	struct du_cie *cie;

	cie = kmem_cache_alloc(frames->kmem_cie, GFP_KERNEL);
	if (!cie)
		return -ENOMEM;

	*cie = *templ;
	return frame_add(&frames->rb_root_cie, &cie->frame, cie_cmp);
}

int du_fde_add(struct du_frames *frames, struct du_fde *templ)
{
	struct du_fde *fde;

	fde = kmem_cache_alloc(frames->kmem_fde, GFP_KERNEL);
	if (!fde)
		return -ENOMEM;

	*fde = *templ;
	return frame_add(&frames->rb_root_fde, &fde->frame, fde_cmp);
}

typedef int (frame_lookup_cb)(struct du_frame *frame, void *data);

static int fde_lookup(struct du_frame *frame, void *data)
{
	struct du_fde *fde = container_of(frame, struct du_fde, frame);
	u8 *addr = (u8 *) data;

	if (addr < fde->loc_start)
		return -1;
	else if (addr > fde->loc_end)
		return 1;
	else
		return 0;
}

static int cie_lookup(struct du_frame *frame, void *data)
{
	struct du_cie *cie = container_of(frame, struct du_cie, frame);
	u8 *addr = (u8 *) data;

	return cie->addr - addr;
}

static struct du_frame*
frame_lookup(struct rb_root *root, frame_lookup_cb cmp, void *data)
{
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;

	while (*p != NULL) {
		struct du_frame *f;
		int ret;

		parent = *p;
		f = rb_entry(parent, struct du_frame, rb_node);

		ret = cmp(f, data);
		if (ret < 0)
			p = &(*p)->rb_left;
		else if (ret > 0)
			p = &(*p)->rb_right;
		else
			return f;
	}

	return NULL;
}

struct du_cie* du_cie_lookup(struct du_frames *frames, u8 *addr)
{
	struct du_frame *frame;

	frame = frame_lookup(&frames->rb_root_cie, cie_lookup, addr);
	return frame ? container_of(frame, struct du_cie, frame) : NULL;
}

struct du_fde *du_fde_lookup(struct du_frames *frames, unsigned long addr)
{
	struct du_frame *frame;

	frame = frame_lookup(&frames->rb_root_fde, fde_lookup, (void*) addr);
	return frame ? container_of(frame, struct du_fde, frame) : NULL;
}

#ifdef CONFIG_DWARF_UNWIND_DEBUGFS
static void *frames_first(struct seq_file *m)
{
	struct rb_root *rb_root = m->private;
	return rb_first(rb_root);
}

static void *frames_next(struct seq_file *m, void *p, loff_t *pos)
{
	(*pos)++;

	if (p == SEQ_START_TOKEN)
		p = frames_first(m);

	return rb_next(p);
}

static void *frames_start(struct seq_file *m, loff_t *pos)
{
	void *p;
	loff_t l;

	if (!*pos)
		return SEQ_START_TOKEN;

	for (p = frames_first(m), l = 1; l < *pos; ) {
		p = frames_next(m, p, &l);
		if (!p)
			break;
	}

	return p;
}

static void frames_stop(struct seq_file *m, void *p)
{
}

static void seq_printf_hexdump(struct seq_file *m, u8 *p, int len)
{
	int i;

	for (i = 0; i < len; i++, p++)
		seq_printf(m, "%02x ", *p & 0xff);
}

static int frames_cie(struct seq_file *m, void *p)
{
	struct du_cie *cie = rb_entry(p, struct du_cie, frame.rb_node);

	if (p == SEQ_START_TOKEN)
		seq_printf(m, "%20s %20s %5s %3s %3s %2s %2s %5s | %-s\n",
			   "offset", "address", "aug_z", "enc", "ret",
			   "ac", "ad", "len", "code");
	else {
		seq_printf(m, "%20lx %20p %5x %3x %3x %2x %2x %5d | ",
			   cie->frame.offset,
			   cie->addr,
			   cie->aug_z,
			   cie->encoding,
			   cie->ret_addr_column,
			   cie->align_code & 0xff,
			   cie->align_data & 0xff,
			   cie->frame.ilen);

		seq_printf_hexdump(m, cie->frame.icode, cie->frame.ilen);
		seq_printf(m, "\n");
	}

	return 0;
}

static int frames_fde(struct seq_file *m, void *p)
{
	struct du_fde *fde = rb_entry(p, struct du_fde, frame.rb_node);

	if (p == SEQ_START_TOKEN)
		seq_printf(m, "%20s %20s %20s %20s %20s %5s | %-s\n",
			   "offset", "address", "cie", "start",
			   "end", "len", "code");
	else {
		struct du_cie *cie = fde->cie;

		seq_printf(m, "%20lx %20p %20p %20p %20p %5d | ",
			   fde->frame.offset,
			   fde,
			   cie->addr,
			   fde->loc_start,
			   fde->loc_end,
			   fde->frame.ilen);

		seq_printf_hexdump(m, fde->frame.icode, fde->frame.ilen);
		seq_printf(m, "\n");
	}

	return 0;
}

static const struct seq_operations cie_seq_ops = {
	.start          = frames_start,
	.next           = frames_next,
	.stop           = frames_stop,
	.show           = frames_cie,
};

static const struct seq_operations fde_seq_ops = {
	.start          = frames_start,
	.next           = frames_next,
	.stop           = frames_stop,
	.show           = frames_fde,
};

static int frames_open(struct inode *inode, struct file *file,
		       const struct seq_operations *seq_ops)
{
	int ret;

	ret = seq_open(file, seq_ops);
	if (!ret) {
		struct seq_file *m = file->private_data;
		m->private = inode->i_private;
        }

	return ret;
}

static int frames_open_cie(struct inode *inode, struct file *file)
{
	return frames_open(inode, file, &cie_seq_ops);
}

static int frames_open_fde(struct inode *inode, struct file *file)
{
	return frames_open(inode, file, &fde_seq_ops);
}

static const struct file_operations frames_ops_cie = {
	.open           = frames_open_cie,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = seq_release,
};

static const struct file_operations frames_ops_fde = {
	.open           = frames_open_fde,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = seq_release,
};

static void debugfs_release(struct du_frames *frames)
{
	debugfs_remove(frames->d_mod);
	debugfs_remove(frames->d_cie);
	debugfs_remove(frames->d_fde);
}

static struct dentry *debugfs_d_frames(void)
{
	static struct dentry *d_frames;

	if (!d_frames)
		d_frames = debugfs_create_dir("unwind_frames", NULL);

	return d_frames;
}

static const char *mod_name(struct du_frames *frames)
{
	struct module *mod = frames->mod;
	return mod ? mod->name : "vmlinux";
}

static int debugfs_init(struct du_frames *frames)
{
	struct dentry *d_frames;
	struct dentry *d_mod, *d_cie, *d_fde;

	d_frames = debugfs_d_frames();
	if (!d_frames)
		return -EINVAL;

	d_mod = debugfs_create_dir(mod_name(frames), d_frames);
	if (!d_mod)
		return -EINVAL;

	d_cie = debugfs_create_file("cie", 0644, d_mod,
				    &frames->rb_root_cie,
				    &frames_ops_cie);
	d_fde = debugfs_create_file("fde", 0644, d_mod,
				    &frames->rb_root_fde,
				    &frames_ops_fde);
	if (!d_cie || !d_fde)
		goto err;

	frames->d_mod = d_mod;
	frames->d_cie = d_cie;
	frames->d_fde = d_fde;
	return 0;

 err:
	debugfs_release(frames);
	return -EINVAL;
}
#else
static int debugfs_init(struct du_frames *frames)
{
	return 0;
}

static int debugfs_release(struct du_frames *frames)
{
	return 0;
}
#endif /* CONFIG_DWARF_UNWIND_DEBUGFS */

#ifdef CONFIG_DWARF_UNWIND_EH_FRAMES
static int frames_source_init(struct du_frames *frames)
{
	return du_ehframe_init(frames);
}

static void frames_source_release(struct du_frames *frames)
{
	du_ehframe_release(frames);
}
#else
static int frames_source_init(struct du_frames *frames)
{
	return -ENODEV;
}

static void frames_source_release(struct du_frames *frames)
{
}
#endif /* CONFIG_DWARF_UNWIND_EH_FRAMES */

static int __frames_init(struct du_frames *frames)
{
	int ret;

	frames->kmem_cie = KMEM_CACHE(du_cie, SLAB_PANIC);
	frames->kmem_fde = KMEM_CACHE(du_fde, SLAB_PANIC);

	ret = frames_source_init(frames);
	if (ret)
		goto out;

	ret = debugfs_init(frames);

 out:
	return ret;
}

static int frames_init(struct module *mod)
{
	struct du_frames *frames;
	unsigned long flags;
	int ret;

	frames = frames_alloc(mod);
	if (!frames)
		return -ENOMEM;

	ret = __frames_init(frames);
	if (ret)
		goto out;

	spin_lock_irqsave(&frames_lock, flags);
	if (frames_find(mod))
		ret = -EINVAL;
	else
		list_add_tail(&frames->list, &frames_list);
	spin_unlock_irqrestore(&frames_lock, flags);

	WARN_ON(ret);

 out:
	if (ret)
		du_frames_put(frames);

	return ret;
}

static void frames_free(struct du_frames *frames)
{
	debugfs_release(frames);
	frames_source_release(frames);

	kmem_cache_destroy(frames->kmem_cie);
	kmem_cache_destroy(frames->kmem_fde);

	kfree(frames);
}

static void frames_release(struct module *mod)
{
	struct du_frames *frames;
	unsigned long flags;

	spin_lock_irqsave(&frames_lock, flags);
	frames = frames_find(mod);
	if (frames)
		list_del(&frames->list);
	spin_unlock_irqrestore(&frames_lock, flags);

	if (frames)
		du_frames_put(frames);

	WARN_ON(!frames);
}

#ifdef CONFIG_MODULES
static int module_notify(struct notifier_block *self,
			 unsigned long val, void *data)
{
	struct module *mod = data;
	int ret = 0;

	switch (val) {
	case MODULE_STATE_COMING:
		ret = frames_init(mod);
		break;

	case MODULE_STATE_GOING:
		frames_release(mod);
		break;
	}

	if (ret)
		printk("Failed to initialize dwarf unwind for module %s\n",
		       mod->name);

	return 0;
}
#else
#define module_notify NULL
#endif /* CONFIG_MODULES */

struct notifier_block module_nb = {
	.notifier_call = module_notify,
	.priority = 0,
};

__init static int init_frames(void)
{
	int ret;

	ret = frames_init(NULL);
	if (ret)
		goto out;

	ret = register_module_notifier(&module_nb);
	if (ret)
		frames_release(NULL);

 out:
	return ret;
}

late_initcall(init_frames);
