#include "io_netcdf.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Format an attribute value as a human-readable string */
static char *format_attr_value(int ncid, int varid, const char *name,
                               nc_type type, size_t len)
{
    char *buf = NULL;
    size_t bufsize = 0;

    if (type == NC_CHAR) {
        /* Text attribute */
        buf = ked_malloc(len + 1);
        if (nc_get_att_text(ncid, varid, name, buf) != NC_NOERR) {
            free(buf);
            return NULL;
        }
        buf[len] = '\0';
        return buf;
    }

    if (type == NC_STRING) {
        char **strs = ked_malloc(len * sizeof(char *));
        if (nc_get_att_string(ncid, varid, name, strs) != NC_NOERR) {
            free(strs);
            return NULL;
        }
        /* Join with ", " */
        bufsize = 1;
        for (size_t i = 0; i < len; i++) {
            bufsize += strlen(strs[i]) + 2;
        }
        buf = ked_malloc(bufsize);
        buf[0] = '\0';
        for (size_t i = 0; i < len; i++) {
            if (i > 0) strcat(buf, ", ");
            strcat(buf, strs[i]);
        }
        nc_free_string(len, strs);
        free(strs);
        return buf;
    }

    /* Numeric attribute - read as doubles */
    double *vals = ked_malloc(len * sizeof(double));
    if (nc_get_att_double(ncid, varid, name, vals) != NC_NOERR) {
        free(vals);
        return NULL;
    }

    /* Format: up to 8 values, then "..." */
    bufsize = len * 32 + 16;
    buf = ked_malloc(bufsize);
    buf[0] = '\0';
    size_t show = len < 8 ? len : 8;
    for (size_t i = 0; i < show; i++) {
        char tmp[32];
        /* Use integer format if value is integral */
        if (vals[i] == (double)(long long)vals[i] &&
            vals[i] > -1e15 && vals[i] < 1e15) {
            snprintf(tmp, sizeof(tmp), "%lld", (long long)vals[i]);
        } else {
            snprintf(tmp, sizeof(tmp), "%g", vals[i]);
        }
        if (i > 0) strcat(buf, ", ");
        strcat(buf, tmp);
    }
    if (len > 8) strcat(buf, ", ...");
    free(vals);
    return buf;
}

/* Read attributes for a variable (or global if varid == NC_GLOBAL) */
static ked_attr_t *read_attrs(int ncid, int varid, int natts)
{
    if (natts <= 0) return NULL;
    ked_attr_t *atts = ked_calloc((size_t)natts, sizeof(ked_attr_t));
    for (int i = 0; i < natts; i++) {
        nc_inq_attname(ncid, varid, i, atts[i].name);
        nc_inq_att(ncid, varid, atts[i].name, &atts[i].type, &atts[i].len);
        atts[i].value_str = format_attr_value(ncid, varid, atts[i].name,
                                              atts[i].type, atts[i].len);
    }
    return atts;
}

static void free_attrs(ked_attr_t *atts, int natts)
{
    if (!atts) return;
    for (int i = 0; i < natts; i++) {
        free(atts[i].value_str);
    }
    free(atts);
}

ked_dataset_t *ked_nc_open(const char *path)
{
    int ncid;
    int rc = nc_open(path, NC_NOWRITE, &ncid);
    if (rc != NC_NOERR) {
        fprintf(stderr, "ked: cannot open '%s': %s\n", path, nc_strerror(rc));
        return NULL;
    }

    ked_dataset_t *ds = ked_calloc(1, sizeof(ked_dataset_t));
    snprintf(ds->path, sizeof(ds->path), "%s", path);
    ds->ncid = ncid;

    /* Query format */
    nc_inq_format(ncid, &ds->format);

    /* Query counts */
    int nunlim = 0;
    int *unlim_ids = NULL;
    nc_inq(ncid, &ds->ndims, &ds->nvars, &ds->ngatts, NULL);

    /* Get unlimited dimension IDs */
    nc_inq_unlimdims(ncid, &nunlim, NULL);
    if (nunlim > 0) {
        unlim_ids = ked_malloc((size_t)nunlim * sizeof(int));
        nc_inq_unlimdims(ncid, NULL, unlim_ids);
    }

    /* Read dimensions */
    ds->dims = ked_calloc((size_t)ds->ndims, sizeof(ked_dim_t));
    for (int d = 0; d < ds->ndims; d++) {
        ds->dims[d].dimid = d;
        nc_inq_dim(ncid, d, ds->dims[d].name, &ds->dims[d].len);
        ds->dims[d].unlimited = false;
        for (int u = 0; u < nunlim; u++) {
            if (unlim_ids[u] == d) {
                ds->dims[d].unlimited = true;
                break;
            }
        }
    }
    free(unlim_ids);

    /* Read variables */
    ds->vars = ked_calloc((size_t)ds->nvars, sizeof(ked_var_t));
    for (int v = 0; v < ds->nvars; v++) {
        ds->vars[v].varid = v;
        nc_inq_var(ncid, v, ds->vars[v].name, &ds->vars[v].type,
                   &ds->vars[v].ndims, ds->vars[v].dimids, &ds->vars[v].natts);

        /* Resolve dimension sizes for shape */
        for (int d = 0; d < ds->vars[v].ndims; d++) {
            int did = ds->vars[v].dimids[d];
            ds->vars[v].shape[d] = ds->dims[did].len;
        }

        /* Read variable attributes */
        ds->vars[v].atts = read_attrs(ncid, v, ds->vars[v].natts);
    }

    /* Read global attributes */
    ds->gatts = read_attrs(ncid, NC_GLOBAL, ds->ngatts);

    return ds;
}

void ked_dataset_close(ked_dataset_t *ds)
{
    if (!ds) return;
    nc_close(ds->ncid);
    for (int v = 0; v < ds->nvars; v++) {
        free_attrs(ds->vars[v].atts, ds->vars[v].natts);
    }
    free(ds->vars);
    free(ds->dims);
    free_attrs(ds->gatts, ds->ngatts);
    free(ds);
}
