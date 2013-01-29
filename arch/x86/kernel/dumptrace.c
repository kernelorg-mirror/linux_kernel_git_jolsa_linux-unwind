
#include <linux/export.h>
#include <asm/stacktrace.h>

dump_trace_t dump_trace = dump_trace_legacy;

EXPORT_SYMBOL(dump_trace);
