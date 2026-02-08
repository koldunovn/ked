#include "ked.h"
#include "cli.h"
#include "ops_data.h"
#include "ops_info.h"
#include "term.h"
#include "util.h"

#include <string.h>

int main(int argc, char **argv)
{
    ked_args_t args;
    if (ked_parse_args(argc, argv, &args) != 0) {
        ked_print_help();
        return 1;
    }

    if (args.help) {
        ked_print_help();
        return 0;
    }

    if (args.version) {
        ked_print_version();
        return 0;
    }

    /* Initialize terminal */
    term_init();
    if (args.no_color) {
        term_set_no_color();
    }

    /* Dispatch operator */
    if (args.operator == NULL) {
        ked_print_help();
        return 1;
    }

    if (strcmp(args.operator, "info") == 0) {
        if (args.nfiles < 1) {
            ked_die("'info' requires at least one input file");
        }
        int rc = 0;
        for (int i = 0; i < args.nfiles; i++) {
            if (ked_op_info(args.files[i]) != 0) rc = 1;
        }
        return rc;
    }

    if (strcmp(args.operator, "sinfo") == 0) {
        if (args.nfiles < 1) {
            ked_die("'sinfo' requires at least one input file");
        }
        int rc = 0;
        for (int i = 0; i < args.nfiles; i++) {
            if (ked_op_sinfo(args.files[i]) != 0) rc = 1;
        }
        return rc;
    }

    if (strcmp(args.operator, "copy") == 0) {
        if (args.nfiles < 2) {
            ked_die("'copy' requires input and output files: ked copy input output");
        }
        return ked_op_copy(args.files[0], args.files[args.nfiles - 1],
                          args.compress);
    }

    if (strcmp(args.operator, "select") == 0 || strcmp(args.operator, "sel") == 0) {
        if (args.nfiles < 2) {
            ked_die("'select' requires input and output files: ked select -v var input output");
        }
        if (!args.var_select) {
            ked_die("'select' requires -v flag: ked select -v temperature input.nc output.nc");
        }
        return ked_op_select(args.files[0], args.files[args.nfiles - 1],
                             args.var_select, args.compress);
    }

    if (strcmp(args.operator, "merge") == 0) {
        if (args.nfiles < 2) {
            ked_die("'merge' requires at least one input and one output file");
        }
        return ked_op_merge(args.nfiles - 1, args.files,
                            args.files[args.nfiles - 1], args.compress);
    }

    if (strcmp(args.operator, "cat") == 0) {
        if (args.nfiles < 2) {
            ked_die("'cat' requires at least one input and one output file");
        }
        return ked_op_cat(args.nfiles - 1, args.files,
                          args.files[args.nfiles - 1], args.compress);
    }

    ked_die("unknown operator '%s'. Run '%s --help' for usage.",
            args.operator, KED_NAME);
}
