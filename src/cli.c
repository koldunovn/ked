#include "cli.h"
#include "ked.h"

#include <stdio.h>
#include <string.h>

void ked_print_help(void)
{
    printf("Usage: %s <operator> [options] <files...>\n", KED_NAME);
    printf("\n");
    printf("Operators:\n");
    printf("  info    Show detailed file information\n");
    printf("  sinfo   Show short summary (one line per variable)\n");
    printf("\n");
    printf("Options:\n");
    printf("  --help       Show this help\n");
    printf("  --version    Show version\n");
    printf("  --no-color   Disable colored output\n");
    printf("  -v <name>    Select variable (future)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s info climate.nc\n", KED_NAME);
    printf("  %s sinfo output.nc\n", KED_NAME);
}

void ked_print_version(void)
{
    printf("%s %s\n", KED_NAME, KED_VERSION);
}

int ked_parse_args(int argc, char **argv, ked_args_t *args)
{
    memset(args, 0, sizeof(*args));

    if (argc < 2) {
        return -1;
    }

    /* Collect files in a temporary array (max = argc) */
    static const char *file_buf[4096];
    int nfiles = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            args->help = true;
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            args->version = true;
            return 0;
        }
        if (strcmp(argv[i], "--no-color") == 0) {
            args->no_color = true;
            continue;
        }
        if (strcmp(argv[i], "-v") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ked: -v requires an argument\n");
                return -1;
            }
            args->var_select = argv[++i];
            continue;
        }

        /* First non-option argument is the operator */
        if (args->operator == NULL) {
            args->operator = argv[i];
            continue;
        }

        /* Remaining arguments are files */
        file_buf[nfiles++] = argv[i];
    }

    args->files = file_buf;
    args->nfiles = nfiles;
    return 0;
}
