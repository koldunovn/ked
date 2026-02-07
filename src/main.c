#include "ked.h"
#include "cli.h"
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

    ked_die("unknown operator '%s'. Run '%s --help' for usage.",
            args.operator, KED_NAME);
}
