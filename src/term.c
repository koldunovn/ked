#include "term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

static bool color_enabled = false;

static const char *ansi_codes[] = {
    [KED_CLR_RESET]   = "\033[0m",
    [KED_CLR_BOLD]    = "\033[1m",
    [KED_CLR_DIM]     = "\033[2m",
    [KED_CLR_RED]     = "\033[31m",
    [KED_CLR_GREEN]   = "\033[32m",
    [KED_CLR_YELLOW]  = "\033[33m",
    [KED_CLR_BLUE]    = "\033[34m",
    [KED_CLR_MAGENTA] = "\033[35m",
    [KED_CLR_CYAN]    = "\033[36m",
    [KED_CLR_WHITE]   = "\033[37m",
};

void term_init(void)
{
    /* Respect NO_COLOR convention (https://no-color.org/) */
    if (getenv("NO_COLOR") != NULL) {
        color_enabled = false;
        return;
    }

    /* Check if stdout is a terminal */
    if (!isatty(STDOUT_FILENO)) {
        color_enabled = false;
        return;
    }

    /* Check TERM is set and not "dumb" */
    const char *term = getenv("TERM");
    if (term == NULL || strcmp(term, "dumb") == 0) {
        color_enabled = false;
        return;
    }

    color_enabled = true;
}

void term_set_no_color(void)
{
    color_enabled = false;
}

const char *term_color(ked_color_t c)
{
    if (!color_enabled) return "";
    return ansi_codes[c];
}

const char *term_bold(void)
{
    return term_color(KED_CLR_BOLD);
}

const char *term_dim(void)
{
    return term_color(KED_CLR_DIM);
}

const char *term_reset(void)
{
    return term_color(KED_CLR_RESET);
}

bool term_has_color(void)
{
    return color_enabled;
}

void term_header(const char *title)
{
    printf("\n%s%s%s\n", term_bold(), title, term_reset());
}

void term_kv(const char *key, const char *fmt, ...)
{
    printf("  %s%-16s%s ", term_color(KED_CLR_CYAN), key, term_reset());
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void term_sep(void)
{
    printf("  %s────────────────────────────────────────%s\n",
           term_dim(), term_reset());
}
