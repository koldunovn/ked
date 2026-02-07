#ifndef KED_CLI_H
#define KED_CLI_H

#include <stdbool.h>

typedef struct {
    const char *operator;       /* "info", "sinfo", etc. */
    int         nfiles;
    const char **files;         /* input file paths */
    bool        no_color;
    bool        help;
    bool        version;
    const char *var_select;     /* -v variable selection (future) */
} ked_args_t;

/* Parse command-line arguments. Returns 0 on success, -1 on error.
 * On --help or --version, sets the corresponding flag and returns 0. */
int ked_parse_args(int argc, char **argv, ked_args_t *args);

/* Print usage/help text */
void ked_print_help(void);

/* Print version */
void ked_print_version(void);

#endif /* KED_CLI_H */
