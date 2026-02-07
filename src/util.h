#ifndef KED_UTIL_H
#define KED_UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdnoreturn.h>

/* Checked malloc - exits on failure */
void *ked_malloc(size_t size);

/* Checked calloc - exits on failure */
void *ked_calloc(size_t count, size_t size);

/* Print error message to stderr and exit */
noreturn void ked_die(const char *fmt, ...);

/* Print warning message to stderr */
void ked_warn(const char *fmt, ...);

/* Format a byte count as human-readable string (writes to static buffer) */
const char *ked_fmt_size(size_t bytes);

#endif /* KED_UTIL_H */
