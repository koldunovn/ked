#include "ops_info.h"
#include "dataset.h"
#include "io_netcdf.h"
#include "term.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

/* Format shape as "time x lat x lon" using dimension names */
static void print_shape(const ked_dataset_t *ds, const ked_var_t *var)
{
    for (int d = 0; d < var->ndims; d++) {
        int did = var->dimids[d];
        if (d > 0) printf(" x ");
        printf("%s%s%s(%s%zu%s)",
               term_color(KED_CLR_YELLOW), ds->dims[did].name, term_reset(),
               term_dim(), var->shape[d], term_reset());
    }
    if (var->ndims == 0) {
        printf("scalar");
    }
}

int ked_op_info(const char *path)
{
    ked_dataset_t *ds = ked_nc_open(path);
    if (!ds) return 1;

    /* File header */
    term_header("File Information");
    term_kv("File:", "%s", ds->path);
    term_kv("Format:", "%s", ked_format_name(ds->format));
    term_kv("Dimensions:", "%d", ds->ndims);
    term_kv("Variables:", "%d", ds->nvars);
    term_kv("Global attrs:", "%d", ds->ngatts);

    /* Dimensions */
    term_header("Dimensions");
    for (int d = 0; d < ds->ndims; d++) {
        ked_dim_t *dim = &ds->dims[d];
        printf("  %s%-20s%s %s%zu%s",
               term_color(KED_CLR_YELLOW), dim->name, term_reset(),
               term_bold(), dim->len, term_reset());
        if (dim->unlimited) {
            printf("  %s(unlimited)%s", term_color(KED_CLR_MAGENTA),
                   term_reset());
        }
        printf("\n");
    }

    /* Variables */
    term_header("Variables");
    for (int v = 0; v < ds->nvars; v++) {
        ked_var_t *var = &ds->vars[v];
        size_t sz = ked_var_size(var);

        printf("  %s%-8s%s %s%-20s%s ",
               term_color(KED_CLR_GREEN), ked_type_name(var->type),
               term_reset(),
               term_bold(), var->name, term_reset());
        print_shape(ds, var);
        printf("  %s[%s]%s", term_dim(), ked_fmt_size(sz), term_reset());
        printf("\n");

        /* Variable attributes */
        for (int a = 0; a < var->natts; a++) {
            ked_attr_t *att = &var->atts[a];
            printf("    %s%s%s: %s\n",
                   term_color(KED_CLR_CYAN), att->name, term_reset(),
                   att->value_str ? att->value_str : "(empty)");
        }
    }

    /* Global attributes */
    if (ds->ngatts > 0) {
        term_header("Global Attributes");
        for (int a = 0; a < ds->ngatts; a++) {
            ked_attr_t *att = &ds->gatts[a];
            printf("  %s%s%s: %s\n",
                   term_color(KED_CLR_CYAN), att->name, term_reset(),
                   att->value_str ? att->value_str : "(empty)");
        }
    }

    printf("\n");
    ked_dataset_close(ds);
    return 0;
}

int ked_op_sinfo(const char *path)
{
    ked_dataset_t *ds = ked_nc_open(path);
    if (!ds) return 1;

    /* Header line */
    printf("%s%-20s %-8s %-40s %s%s\n",
           term_bold(),
           "Variable", "Type", "Shape", "Size",
           term_reset());
    term_sep();

    for (int v = 0; v < ds->nvars; v++) {
        ked_var_t *var = &ds->vars[v];

        /* Build shape string */
        char shape_str[256] = "";
        for (int d = 0; d < var->ndims; d++) {
            char tmp[64];
            int did = var->dimids[d];
            snprintf(tmp, sizeof(tmp), "%s%s(%zu)",
                     d > 0 ? " x " : "",
                     ds->dims[did].name, var->shape[d]);
            strncat(shape_str, tmp, sizeof(shape_str) - strlen(shape_str) - 1);
        }
        if (var->ndims == 0) {
            snprintf(shape_str, sizeof(shape_str), "scalar");
        }

        size_t sz = ked_var_size(var);
        printf("%-20s %s%-8s%s %-40s %s\n",
               var->name,
               term_color(KED_CLR_GREEN), ked_type_name(var->type),
               term_reset(),
               shape_str,
               ked_fmt_size(sz));
    }

    printf("\n%s%s%s: %d dims, %d vars, %d global attrs\n",
           term_dim(), ds->path, term_reset(),
           ds->ndims, ds->nvars, ds->ngatts);

    ked_dataset_close(ds);
    return 0;
}
