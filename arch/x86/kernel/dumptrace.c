
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/kobject.h>
#include <asm/stacktrace.h>
#include <asm/ptrace.h>

dump_trace_t dump_trace = dump_trace_legacy;

EXPORT_SYMBOL(dump_trace);

#ifdef CONFIG_DWARF_UNWIND
struct backtrace {
	const char	*name;
	dump_trace_t	fn;
};

enum {
	BACKTRACE_LEGACY = 0,
	BACKTRACE_DWARF  = 1,
};

static struct backtrace bt[] = {
	[BACKTRACE_LEGACY] = {
		.name	= "legacy",
		.fn	= dump_trace_legacy,
	},
	[BACKTRACE_DWARF] = {
		.name	= "dwarf",
		.fn	= dump_trace_dwarf,
	},
};

static int bt_idx;
static DEFINE_SPINLOCK(bt_lock);

int backtrace_switch(int i)
{
	struct backtrace *b = &bt[i];
	unsigned long flags;

	spin_lock_irqsave(&bt_lock, flags);
	dump_trace = b->fn;
	bt_idx = i;
	spin_unlock_irqrestore(&bt_lock, flags);
	return 0;
}

ssize_t backtrace_show(struct kobject *kobj,
		       struct kobj_attribute *attr,
		       char *buf)
{
	struct backtrace *b = &bt[bt_idx];
	return sprintf(buf, "%s\n", b->name);
}

ssize_t backtrace_store(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	int i, ret = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(bt); i++) {
		struct backtrace *b = &bt[i];

		if (!strncmp(buf, b->name, strlen(b->name))) {
			ret = backtrace_switch(i);
			break;
		}
	}

	return ret < 0 ? ret : count;
}
#endif /* CONFIG_DWARF_UNWIND */
