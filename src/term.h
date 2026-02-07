#ifndef KED_TERM_H
#define KED_TERM_H

#include <stdbool.h>

/* Color codes */
typedef enum {
    KED_CLR_RESET = 0,
    KED_CLR_BOLD,
    KED_CLR_DIM,
    KED_CLR_RED,
    KED_CLR_GREEN,
    KED_CLR_YELLOW,
    KED_CLR_BLUE,
    KED_CLR_MAGENTA,
    KED_CLR_CYAN,
    KED_CLR_WHITE,
} ked_color_t;

/* Initialize terminal (detect color support). Call once at startup. */
void term_init(void);

/* Force color off (--no-color flag) */
void term_set_no_color(void);

/* Return ANSI escape string for a color, or "" if color is disabled */
const char *term_color(ked_color_t c);

/* Convenience: bold text */
const char *term_bold(void);

/* Convenience: dim text */
const char *term_dim(void);

/* Convenience: reset all formatting */
const char *term_reset(void);

/* Print a section header */
void term_header(const char *title);

/* Print a key-value pair with aligned columns */
void term_kv(const char *key, const char *fmt, ...);

/* Print a separator line */
void term_sep(void);

/* Is color enabled? */
bool term_has_color(void);

#endif /* KED_TERM_H */
