#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/hash.h>
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

static int frames_source_init(struct du_frames *frames)
{
	return -ENODEV;
}

static void frames_source_release(struct du_frames *frames)
{
}

static int __frames_init(struct du_frames *frames)
{
	frames->kmem_cie = KMEM_CACHE(du_cie, SLAB_PANIC);
	frames->kmem_fde = KMEM_CACHE(du_fde, SLAB_PANIC);

	return frames_source_init(frames);
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
