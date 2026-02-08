#include "cli.h"
#include "ked.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ked_print_help(void)
{
    printf("Usage: %s <operator> [options] <files...>\n", KED_NAME);
    printf("\n");
    printf("Operators:\n");
    printf("  info    Show detailed file information\n");
    printf("  sinfo   Show short summary (one line per variable)\n");
    printf("  copy    Copy/convert file (e.g., GRIB to netCDF)\n");
    printf("  select  Subset by variable name (-v)\n");
    printf("  merge   Combine variables from multiple files\n");
    printf("  cat     Concatenate files along time dimension\n");
    printf("\n");
    printf("Options:\n");
    printf("  --help       Show this help\n");
    printf("  --version    Show version\n");
    printf("  --no-color   Disable colored output\n");
    printf("  -v <name>    Select variable(s), comma-separated\n");
    printf("  -z <level>   Compression: 0=off, 1-9=deflate (default: auto)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s info climate.nc\n", KED_NAME);
    printf("  %s sinfo output.nc\n", KED_NAME);
    printf("  %s copy input.grib2 output.nc\n", KED_NAME);
    printf("  %s select -v temperature input.nc output.nc\n", KED_NAME);
    printf("  %s merge a.nc b.nc merged.nc\n", KED_NAME);
    printf("  %s cat jan.nc feb.nc year.nc\n", KED_NAME);
}

void ked_print_version(void)
{
    printf("%s %s\n", KED_NAME, KED_VERSION);
}

int ked_parse_args(int argc, char **argv, ked_args_t *args)
{
    memset(args, 0, sizeof(*args));
    args->compress = -1; /* auto: preserve source compression */

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
        if (strcmp(argv[i], "-z") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ked: -z requires an argument (0-9)\n");
                return -1;
            }
            args->compress = atoi(argv[++i]);
            if (args->compress < 0 || args->compress > 9) {
                fprintf(stderr, "ked: -z level must be 0-9\n");
                return -1;
            }
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
