#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void *ked_malloc(size_t size)
{
    void *p = malloc(size);
    if (!p && size > 0) {
        ked_die("out of memory (requested %zu bytes)", size);
    }
    return p;
}

void *ked_calloc(size_t count, size_t size)
{
    void *p = calloc(count, size);
    if (!p && count > 0 && size > 0) {
        ked_die("out of memory (requested %zu * %zu bytes)", count, size);
    }
    return p;
}

noreturn void ked_die(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "ked: error: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

void ked_warn(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "ked: warning: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

const char *ked_fmt_size(size_t bytes)
{
    static char buf[64];
    if (bytes < 1024) {
        snprintf(buf, sizeof(buf), "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f KB", (double)bytes / 1024.0);
    } else if (bytes < 1024UL * 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buf, sizeof(buf), "%.2f GB",
                 (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}
