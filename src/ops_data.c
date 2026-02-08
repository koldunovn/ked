#include "ops_data.h"
#include "dataset.h"
#include "io_netcdf.h"
#include "util.h"

#include <math.h>
#include <netcdf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Utility helpers
 * ================================================================ */

/* Check if a variable name is in a comma-separated list */
static int name_in_list(const char *name, const char *list)
{
    const char *p = list;
    size_t nlen = strlen(name);
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t seg = comma ? (size_t)(comma - p) : strlen(p);
        if (seg == nlen && strncmp(p, name, seg) == 0)
            return 1;
        if (!comma) break;
        p = comma + 1;
    }
    return 0;
}

/* Check if a variable is a coordinate variable (1D, shares name with its dim) */
static int is_coord_var(const ked_dataset_t *ds, const ked_var_t *var)
{
    if (var->ndims != 1) return 0;
    int did = var->dimids[0];
    return strcmp(var->name, ds->dims[did].name) == 0;
}

/* Find dimension index by name, returns -1 if not found */
static int find_dim_by_name(const ked_dataset_t *ds, const char *name)
{
    for (int d = 0; d < ds->ndims; d++) {
        if (strcmp(ds->dims[d].name, name) == 0)
            return d;
    }
    return -1;
}

/* Deep-copy attributes (including raw binary data) */
static ked_attr_t *copy_attrs(const ked_attr_t *src, int natts)
{
    if (natts <= 0 || !src) return NULL;
    ked_attr_t *dst = ked_calloc((size_t)natts, sizeof(ked_attr_t));
    for (int i = 0; i < natts; i++) {
        dst[i] = src[i];
        dst[i].value_str = src[i].value_str ? strdup(src[i].value_str) : NULL;
        if (src[i].value_raw) {
            size_t raw_sz = ked_type_size(src[i].type) * src[i].len;
            if (raw_sz > 0) {
                dst[i].value_raw = ked_malloc(raw_sz);
                memcpy(dst[i].value_raw, src[i].value_raw, raw_sz);
            } else {
                dst[i].value_raw = NULL;
            }
        }
    }
    return dst;
}

/* Apply compress override to all variables in a dataset.
 * compress: -1=keep source settings, 0=disable, 1-9=force deflate level */
static void apply_compress(ked_dataset_t *ds, int compress)
{
    if (compress < 0) return; /* auto: keep source settings */
    for (int v = 0; v < ds->nvars; v++) {
        ds->vars[v].deflate = compress;
        ds->vars[v].shuffle = (compress > 0);
    }
}

/* ================================================================
 * CF time unit parsing and conversion
 * ================================================================ */

typedef struct {
    double step_days;   /* Unit step size in fractional days */
    double ref_jdn;     /* Julian Day Number of reference date */
} time_ref_t;

/* Gregorian calendar date to Julian Day Number */
static double date_to_jdn(int y, int m, int d, int hr, int mn, int sec)
{
    int a = (14 - m) / 12;
    int yy = y + 4800 - a;
    int mm = m + 12 * a - 3;
    double jdn = (double)(d + (153 * mm + 2) / 5 + 365 * yy
                          + yy / 4 - yy / 100 + yy / 400 - 32045);
    jdn += (hr - 12) / 24.0 + mn / 1440.0 + sec / 86400.0;
    return jdn;
}

/* Parse CF time units string: "<step> since <YYYY-MM-DD> [HH:MM:SS]" */
static int parse_time_units(const char *units, time_ref_t *ref)
{
    if (!units) return -1;
    char step[32] = {0};
    int y = 0, m = 1, d = 1, hr = 0, mn = 0, sec = 0;
    int n = sscanf(units, "%31s since %d-%d-%d %d:%d:%d",
                   step, &y, &m, &d, &hr, &mn, &sec);
    if (n < 4) return -1;

    if (strncmp(step, "second", 6) == 0)
        ref->step_days = 1.0 / 86400.0;
    else if (strncmp(step, "minute", 6) == 0)
        ref->step_days = 1.0 / 1440.0;
    else if (strncmp(step, "hour", 4) == 0)
        ref->step_days = 1.0 / 24.0;
    else if (strncmp(step, "day", 3) == 0)
        ref->step_days = 1.0;
    else
        return -1;

    ref->ref_jdn = date_to_jdn(y, m, d, hr, mn, sec);
    return 0;
}

/* Convert time value to absolute Julian Day Number */
static double time_val_to_jdn(double val, const time_ref_t *ref)
{
    return ref->ref_jdn + val * ref->step_days;
}

/* Convert Julian Day Number to time value in given reference */
static double jdn_to_time_val(double jdn, const time_ref_t *ref)
{
    return (jdn - ref->ref_jdn) / ref->step_days;
}

/* Find the unlimited dimension index */
static int find_unlimited_dim(const ked_dataset_t *ds)
{
    for (int d = 0; d < ds->ndims; d++) {
        if (ds->dims[d].unlimited) return d;
    }
    return -1;
}

/* Find time coordinate variable index (1D, matches unlimited dim name) */
static int find_time_var(const ked_dataset_t *ds, int time_did)
{
    if (time_did < 0) return -1;
    for (int v = 0; v < ds->nvars; v++) {
        if (ds->vars[v].ndims == 1 && ds->vars[v].dimids[0] == time_did &&
            strcmp(ds->vars[v].name, ds->dims[time_did].name) == 0)
            return v;
    }
    return -1;
}

/* Get the "units" attribute string from a variable */
static const char *get_var_units(const ked_dataset_t *ds, int varidx)
{
    const ked_var_t *var = &ds->vars[varidx];
    for (int a = 0; a < var->natts; a++) {
        if (strcmp(var->atts[a].name, "units") == 0)
            return var->atts[a].value_str;
    }
    return NULL;
}

/* Read time coordinate values as doubles */
static double *read_time_values(const ked_dataset_t *ds, int varidx, size_t *n)
{
    const ked_var_t *var = &ds->vars[varidx];
    *n = ked_var_nelems(var);
    if (*n == 0) return NULL;

    size_t sz = ked_var_size(var);
    void *raw = ked_malloc(sz);
    if (ked_dataset_read_var(ds, varidx, raw) != 0) {
        free(raw);
        return NULL;
    }

    double *vals = ked_malloc(*n * sizeof(double));
    for (size_t i = 0; i < *n; i++) {
        switch (var->type) {
        case KED_TYPE_FLOAT:  vals[i] = (double)((float *)raw)[i]; break;
        case KED_TYPE_DOUBLE: vals[i] = ((double *)raw)[i]; break;
        case KED_TYPE_INT:    vals[i] = (double)((int *)raw)[i]; break;
        case KED_TYPE_SHORT:  vals[i] = (double)((short *)raw)[i]; break;
        default: vals[i] = 0; break;
        }
    }
    free(raw);
    return vals;
}

/* Convert time values in a raw buffer in-place */
static void convert_time_buf(void *buf, size_t n, ked_type_t type,
                              const time_ref_t *from, const time_ref_t *to)
{
    for (size_t i = 0; i < n; i++) {
        double val;
        if (type == KED_TYPE_DOUBLE) val = ((double *)buf)[i];
        else if (type == KED_TYPE_FLOAT) val = (double)((float *)buf)[i];
        else continue;

        double jdn = time_val_to_jdn(val, from);
        double converted = jdn_to_time_val(jdn, to);

        if (type == KED_TYPE_DOUBLE) ((double *)buf)[i] = converted;
        else ((float *)buf)[i] = (float)converted;
    }
}

/* qsort comparator for doubles */
static int compare_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

/* ================================================================
 * copy: read input, write output as netCDF
 * ================================================================ */
int ked_op_copy(const char *inpath, const char *outpath, int compress)
{
    ked_dataset_t *in = ked_dataset_open(inpath);
    if (!in) return 1;

    /* Build output dataset as a copy of input metadata */
    ked_dataset_t *out = ked_calloc(1, sizeof(ked_dataset_t));
    out->format = KED_FMT_NC4;
    out->ndims = in->ndims;
    out->nvars = in->nvars;
    out->ngatts = in->ngatts;

    out->dims = ked_calloc((size_t)out->ndims, sizeof(ked_dim_t));
    memcpy(out->dims, in->dims, (size_t)out->ndims * sizeof(ked_dim_t));

    out->vars = ked_calloc((size_t)out->nvars, sizeof(ked_var_t));
    for (int v = 0; v < out->nvars; v++) {
        out->vars[v] = in->vars[v];
        out->vars[v].atts = copy_attrs(in->vars[v].atts, in->vars[v].natts);
    }

    out->gatts = copy_attrs(in->gatts, in->ngatts);
    apply_compress(out, compress);

    /* Create output file */
    if (ked_nc_create(out, outpath) != 0) {
        ked_dataset_close(in);
        ked_dataset_close(out);
        return 1;
    }

    /* Copy all variable data */
    for (int v = 0; v < in->nvars; v++) {
        size_t sz = ked_var_size(&in->vars[v]);
        if (sz == 0) continue;
        void *buf = ked_malloc(sz);
        if (ked_dataset_read_var(in, v, buf) != 0) {
            free(buf);
            ked_dataset_close(in);
            ked_dataset_close(out);
            return 1;
        }
        if (ked_nc_write_var(out, v, buf) != 0) {
            free(buf);
            ked_dataset_close(in);
            ked_dataset_close(out);
            return 1;
        }
        free(buf);
    }

    ked_dataset_close(in);
    ked_dataset_close(out);
    return 0;
}

/* ================================================================
 * select: copy only selected variables (+ their coordinate vars)
 * ================================================================ */
int ked_op_select(const char *inpath, const char *outpath,
                  const char *var_names, int compress)
{
    ked_dataset_t *in = ked_dataset_open(inpath);
    if (!in) return 1;

    /* Determine which variables to include */
    int *include = ked_calloc((size_t)in->nvars, sizeof(int));
    int nsel = 0;

    /* Mark selected vars and coordinate vars used by them */
    int *dim_used = ked_calloc((size_t)in->ndims, sizeof(int));

    for (int v = 0; v < in->nvars; v++) {
        if (name_in_list(in->vars[v].name, var_names)) {
            include[v] = 1;
            nsel++;
            for (int d = 0; d < in->vars[v].ndims; d++)
                dim_used[in->vars[v].dimids[d]] = 1;
        }
    }

    if (nsel == 0) {
        ked_warn("no variables matched '%s'", var_names);
        free(include);
        free(dim_used);
        ked_dataset_close(in);
        return 1;
    }

    /* Also include coordinate variables for used dimensions */
    for (int v = 0; v < in->nvars; v++) {
        if (!include[v] && is_coord_var(in, &in->vars[v]) &&
            dim_used[in->vars[v].dimids[0]]) {
            include[v] = 1;
        }
    }

    /* Count included vars */
    int out_nvars = 0;
    for (int v = 0; v < in->nvars; v++)
        if (include[v]) out_nvars++;

    /* Build output dataset with only used dims and selected vars */
    ked_dataset_t *out = ked_calloc(1, sizeof(ked_dataset_t));
    out->format = KED_FMT_NC4;

    /* Copy used dims (preserving order), build dim ID mapping */
    int *dim_map = ked_malloc((size_t)in->ndims * sizeof(int));
    int out_ndims = 0;
    for (int d = 0; d < in->ndims; d++) {
        if (dim_used[d])
            dim_map[d] = out_ndims++;
        else
            dim_map[d] = -1;
    }

    out->ndims = out_ndims;
    out->dims = ked_calloc((size_t)out_ndims, sizeof(ked_dim_t));
    for (int d = 0, od = 0; d < in->ndims; d++) {
        if (dim_used[d]) {
            out->dims[od] = in->dims[d];
            out->dims[od].dimid = od;
            od++;
        }
    }

    /* Copy selected vars with remapped dim IDs */
    out->nvars = out_nvars;
    out->vars = ked_calloc((size_t)out_nvars, sizeof(ked_var_t));
    int *var_map = ked_malloc((size_t)in->nvars * sizeof(int));
    int ov = 0;
    for (int v = 0; v < in->nvars; v++) {
        if (!include[v]) { var_map[v] = -1; continue; }
        var_map[v] = ov;
        out->vars[ov] = in->vars[v];
        out->vars[ov].varid = ov;
        out->vars[ov].atts = copy_attrs(in->vars[v].atts, in->vars[v].natts);
        for (int d = 0; d < out->vars[ov].ndims; d++)
            out->vars[ov].dimids[d] = dim_map[in->vars[v].dimids[d]];
        ov++;
    }

    out->ngatts = in->ngatts;
    out->gatts = copy_attrs(in->gatts, in->ngatts);
    apply_compress(out, compress);

    /* Create output and copy data */
    if (ked_nc_create(out, outpath) != 0) {
        free(include); free(dim_used); free(dim_map); free(var_map);
        ked_dataset_close(in);
        ked_dataset_close(out);
        return 1;
    }

    for (int v = 0; v < in->nvars; v++) {
        if (!include[v]) continue;
        size_t sz = ked_var_size(&in->vars[v]);
        if (sz == 0) continue;
        void *buf = ked_malloc(sz);
        if (ked_dataset_read_var(in, v, buf) != 0) {
            free(buf);
            free(include); free(dim_used); free(dim_map); free(var_map);
            ked_dataset_close(in);
            ked_dataset_close(out);
            return 1;
        }
        if (ked_nc_write_var(out, var_map[v], buf) != 0) {
            free(buf);
            free(include); free(dim_used); free(dim_map); free(var_map);
            ked_dataset_close(in);
            ked_dataset_close(out);
            return 1;
        }
        free(buf);
    }

    free(include);
    free(dim_used);
    free(dim_map);
    free(var_map);
    ked_dataset_close(in);
    ked_dataset_close(out);
    return 0;
}

/* ================================================================
 * merge: combine variables from multiple files into one
 *
 * Handles different time units across files by building a unified
 * time axis and writing data slices at the correct positions.
 * ================================================================ */
int ked_op_merge(int nfiles, const char **inpaths, const char *outpath,
                 int compress)
{
    if (nfiles < 1) ked_die("'merge' requires at least one input file");

    int rc = 1;
    ked_dataset_t **inputs = NULL;
    ked_dataset_t *out = NULL;
    int *src_file = NULL, *src_vidx = NULL;
    double *unified_jdns = NULL;
    int **time_maps = NULL;
    double **file_jdns = NULL;
    size_t *file_ntimes = NULL;

    /* Open all inputs */
    inputs = ked_malloc((size_t)nfiles * sizeof(ked_dataset_t *));
    memset(inputs, 0, (size_t)nfiles * sizeof(ked_dataset_t *));
    for (int f = 0; f < nfiles; f++) {
        inputs[f] = ked_dataset_open(inpaths[f]);
        if (!inputs[f]) goto cleanup;
    }
    ked_dataset_t *first = inputs[0];

    /* --- Collect unique dimensions from all files (by name) --- */
    int max_dims = 0;
    for (int f = 0; f < nfiles; f++) max_dims += inputs[f]->ndims;

    out = ked_calloc(1, sizeof(ked_dataset_t));
    out->format = KED_FMT_NC4;
    out->dims = ked_calloc((size_t)max_dims, sizeof(ked_dim_t));
    int out_ndims = 0;
    int out_time_did = -1;

    for (int f = 0; f < nfiles; f++) {
        for (int d = 0; d < inputs[f]->ndims; d++) {
            /* Check if this dim name already exists in output */
            int existing = -1;
            for (int od = 0; od < out_ndims; od++) {
                if (strcmp(out->dims[od].name, inputs[f]->dims[d].name) == 0) {
                    existing = od;
                    break;
                }
            }
            if (existing >= 0) {
                if (!out->dims[existing].unlimited &&
                    out->dims[existing].len != inputs[f]->dims[d].len) {
                    ked_warn("dimension '%s' size mismatch: %zu vs %zu",
                             inputs[f]->dims[d].name,
                             out->dims[existing].len, inputs[f]->dims[d].len);
                }
                continue;
            }
            out->dims[out_ndims] = inputs[f]->dims[d];
            out->dims[out_ndims].dimid = out_ndims;
            if (inputs[f]->dims[d].unlimited)
                out_time_did = out_ndims;
            out_ndims++;
        }
    }
    out->ndims = out_ndims;

    /* --- Build unified time axis --- */
    size_t unified_ntime = 0;
    time_ref_t out_time_ref = {0};
    int have_time_ref = 0;
    int all_time_valid = 1;

    if (out_time_did >= 0) {
        file_jdns = ked_calloc((size_t)nfiles, sizeof(double *));
        file_ntimes = ked_calloc((size_t)nfiles, sizeof(size_t));

        for (int f = 0; f < nfiles; f++) {
            int tdid = find_unlimited_dim(inputs[f]);
            if (tdid < 0) continue;
            int tvid = find_time_var(inputs[f], tdid);
            if (tvid < 0) { all_time_valid = 0; continue; }

            time_ref_t ref;
            if (parse_time_units(get_var_units(inputs[f], tvid), &ref) != 0) {
                all_time_valid = 0;
                continue;
            }

            if (!have_time_ref) {
                out_time_ref = ref;
                have_time_ref = 1;
            }

            size_t nt = 0;
            double *vals = read_time_values(inputs[f], tvid, &nt);
            if (!vals) { all_time_valid = 0; continue; }

            file_jdns[f] = ked_malloc(nt * sizeof(double));
            file_ntimes[f] = nt;
            for (size_t i = 0; i < nt; i++)
                file_jdns[f][i] = time_val_to_jdn(vals[i], &ref);
            free(vals);
        }

        if (all_time_valid && have_time_ref) {
            /* Collect, sort, and deduplicate all JDNs */
            size_t max_total = 0;
            for (int f = 0; f < nfiles; f++) max_total += file_ntimes[f];

            if (max_total > 0) {
                double *all_jdns = ked_malloc(max_total * sizeof(double));
                size_t njdns = 0;
                for (int f = 0; f < nfiles; f++) {
                    for (size_t i = 0; i < file_ntimes[f]; i++)
                        all_jdns[njdns++] = file_jdns[f][i];
                }

                qsort(all_jdns, njdns, sizeof(double), compare_double);

                /* Deduplicate with 1-second tolerance */
                size_t unique = 0;
                for (size_t i = 0; i < njdns; i++) {
                    if (unique == 0 ||
                        fabs(all_jdns[i] - all_jdns[unique - 1]) >= 1.0 / 86400.0)
                        all_jdns[unique++] = all_jdns[i];
                }

                unified_jdns = all_jdns;
                unified_ntime = unique;

                /* Build time maps: source step → output step */
                time_maps = ked_calloc((size_t)nfiles, sizeof(int *));
                for (int f = 0; f < nfiles; f++) {
                    if (file_ntimes[f] == 0) continue;
                    time_maps[f] = ked_malloc(file_ntimes[f] * sizeof(int));
                    for (size_t i = 0; i < file_ntimes[f]; i++) {
                        time_maps[f][i] = -1;
                        for (size_t j = 0; j < unified_ntime; j++) {
                            if (fabs(file_jdns[f][i] - unified_jdns[j]) <
                                1.0 / 86400.0) {
                                time_maps[f][i] = (int)j;
                                break;
                            }
                        }
                    }
                }

                /* Update output time dim size */
                out->dims[out_time_did].len = unified_ntime;
            }
        }
        /* If time parsing failed, keep first file's time dim size */
    }

    /* --- Collect unique variables, remap dimids --- */
    int total_vars = 0;
    for (int f = 0; f < nfiles; f++) total_vars += inputs[f]->nvars;

    out->vars = ked_calloc((size_t)total_vars, sizeof(ked_var_t));
    out->nvars = 0;
    src_file = ked_malloc((size_t)total_vars * sizeof(int));
    src_vidx = ked_malloc((size_t)total_vars * sizeof(int));

    for (int f = 0; f < nfiles; f++) {
        for (int v = 0; v < inputs[f]->nvars; v++) {
            /* Skip duplicates by name */
            int dup = 0;
            for (int ov2 = 0; ov2 < out->nvars; ov2++) {
                if (strcmp(out->vars[ov2].name, inputs[f]->vars[v].name) == 0) {
                    dup = 1;
                    break;
                }
            }
            if (dup) continue;

            int idx = out->nvars;
            out->vars[idx] = inputs[f]->vars[v];
            out->vars[idx].varid = idx;
            out->vars[idx].atts = copy_attrs(inputs[f]->vars[v].atts,
                                              inputs[f]->vars[v].natts);

            /* Remap dimids from source file to output dims (by name) */
            for (int d = 0; d < out->vars[idx].ndims; d++) {
                int sdid = inputs[f]->vars[v].dimids[d];
                const char *dname = inputs[f]->dims[sdid].name;
                int odid = find_dim_by_name(out, dname);
                if (odid >= 0) {
                    out->vars[idx].dimids[d] = odid;
                    out->vars[idx].shape[d] = out->dims[odid].len;
                }
            }

            src_file[idx] = f;
            src_vidx[idx] = v;
            out->nvars++;
        }
    }

    /* Copy global attrs from first file */
    out->ngatts = first->ngatts;
    out->gatts = copy_attrs(first->gatts, first->ngatts);
    apply_compress(out, compress);

    /* --- Create output file --- */
    if (ked_nc_create(out, outpath) != 0) goto cleanup;

    int ncid = *(int *)out->backend;

    /* --- Write unified time coordinate --- */
    if (unified_jdns && unified_ntime > 0 && have_time_ref) {
        int out_tvid = find_time_var(out, out_time_did);
        if (out_tvid >= 0) {
            ked_var_t *tvar = &out->vars[out_tvid];
            size_t nc_start = 0, nc_count = unified_ntime;

            if (tvar->type == KED_TYPE_DOUBLE) {
                double *tvals = ked_malloc(unified_ntime * sizeof(double));
                for (size_t i = 0; i < unified_ntime; i++)
                    tvals[i] = jdn_to_time_val(unified_jdns[i], &out_time_ref);
                nc_put_vara_double(ncid, tvar->varid, &nc_start, &nc_count,
                                   tvals);
                free(tvals);
            } else {
                float *tvals = ked_malloc(unified_ntime * sizeof(float));
                for (size_t i = 0; i < unified_ntime; i++)
                    tvals[i] = (float)jdn_to_time_val(unified_jdns[i],
                                                       &out_time_ref);
                nc_put_vara_float(ncid, tvar->varid, &nc_start, &nc_count,
                                  tvals);
                free(tvals);
            }

            /* Mark as already written */
            src_file[out_tvid] = -1;
        }
    }

    /* --- Write all other variables --- */
    for (int ov2 = 0; ov2 < out->nvars; ov2++) {
        if (src_file[ov2] < 0) continue; /* already written */

        int f = src_file[ov2];
        int v = src_vidx[ov2];
        ked_var_t *svar = &inputs[f]->vars[v];
        size_t sz = ked_var_size(svar);
        if (sz == 0) continue;

        void *buf = ked_malloc(sz);
        if (ked_dataset_read_var(inputs[f], v, buf) != 0) {
            free(buf);
            continue;
        }

        /* Check if variable has time dimension */
        int time_axis = -1;
        if (out_time_did >= 0) {
            for (int d = 0; d < out->vars[ov2].ndims; d++) {
                if (out->vars[ov2].dimids[d] == out_time_did) {
                    time_axis = d;
                    break;
                }
            }
        }

        if (time_axis == 0 && time_maps && time_maps[f]) {
            /* Time at axis 0: write slice by slice at correct positions */
            size_t slice_elems = 1;
            for (int d = 1; d < svar->ndims; d++)
                slice_elems *= svar->shape[d];
            size_t slice_bytes = slice_elems * ked_type_size(svar->type);

            size_t src_ntime = svar->shape[0];
            for (size_t t = 0; t < src_ntime; t++) {
                int out_t = time_maps[f][t];
                if (out_t < 0) continue;

                size_t nc_start[KED_MAX_DIMS] = {0};
                size_t nc_count[KED_MAX_DIMS] = {0};
                nc_start[0] = (size_t)out_t;
                nc_count[0] = 1;
                for (int d = 1; d < out->vars[ov2].ndims; d++)
                    nc_count[d] = out->vars[ov2].shape[d];

                void *slice = (char *)buf + t * slice_bytes;
                nc_put_vara(ncid, out->vars[ov2].varid,
                            nc_start, nc_count, slice);
            }
        } else if (time_axis >= 0 && time_maps && time_maps[f]) {
            /* Time not at axis 0: write with source shape at start=0 */
            size_t nc_start[KED_MAX_DIMS] = {0};
            size_t nc_count[KED_MAX_DIMS];
            for (int d = 0; d < svar->ndims; d++)
                nc_count[d] = svar->shape[d];
            nc_put_vara(ncid, out->vars[ov2].varid,
                        nc_start, nc_count, buf);
        } else {
            /* No time dimension or no time remapping */
            ked_nc_write_var(out, ov2, buf);
        }

        free(buf);
    }

    rc = 0;

cleanup:
    if (time_maps) {
        for (int f = 0; f < nfiles; f++) free(time_maps[f]);
        free(time_maps);
    }
    free(unified_jdns);
    if (file_jdns) {
        for (int f = 0; f < nfiles; f++) free(file_jdns[f]);
        free(file_jdns);
    }
    free(file_ntimes);
    free(src_file);
    free(src_vidx);
    if (inputs) {
        for (int f = 0; f < nfiles; f++) {
            if (inputs[f]) ked_dataset_close(inputs[f]);
        }
        free(inputs);
    }
    if (out) ked_dataset_close(out);
    return rc;
}

/* ================================================================
 * cat: concatenate files along time (unlimited) dimension
 *
 * Converts time coordinate values to the first file's reference
 * when files use different time units.
 * ================================================================ */
int ked_op_cat(int nfiles, const char **inpaths, const char *outpath,
               int compress)
{
    if (nfiles < 1) {
        ked_die("'cat' requires at least one input file");
    }

    /* Open all input files */
    ked_dataset_t **inputs = ked_malloc((size_t)nfiles * sizeof(ked_dataset_t *));
    for (int f = 0; f < nfiles; f++) {
        inputs[f] = ked_dataset_open(inpaths[f]);
        if (!inputs[f]) {
            for (int g = 0; g < f; g++) ked_dataset_close(inputs[g]);
            free(inputs);
            return 1;
        }
    }

    ked_dataset_t *first = inputs[0];

    /* Find the unlimited (time) dimension */
    int time_did = -1;
    for (int d = 0; d < first->ndims; d++) {
        if (first->dims[d].unlimited) {
            time_did = d;
            break;
        }
    }
    if (time_did < 0) {
        ked_warn("no unlimited dimension found for concatenation");
        for (int f = 0; f < nfiles; f++) ked_dataset_close(inputs[f]);
        free(inputs);
        return 1;
    }

    /* Compute total time length */
    size_t total_time = 0;
    for (int f = 0; f < nfiles; f++) {
        int tdid = find_dim_by_name(inputs[f], first->dims[time_did].name);
        if (tdid < 0) {
            ked_warn("file '%s' missing dimension '%s'",
                     inpaths[f], first->dims[time_did].name);
            for (int g = 0; g < nfiles; g++) ked_dataset_close(inputs[g]);
            free(inputs);
            return 1;
        }
        total_time += inputs[f]->dims[tdid].len;
    }

    /* Parse time units for conversion */
    int time_coord_vidx = find_time_var(first, time_did);
    time_ref_t *cat_time_refs = NULL;
    int cat_need_convert = 0;

    if (time_coord_vidx >= 0) {
        cat_time_refs = ked_calloc((size_t)nfiles, sizeof(time_ref_t));
        int all_valid = 1;

        for (int f = 0; f < nfiles; f++) {
            int tdid_f = find_dim_by_name(inputs[f], first->dims[time_did].name);
            int tvid_f = find_time_var(inputs[f], tdid_f);
            if (tvid_f >= 0) {
                if (parse_time_units(get_var_units(inputs[f], tvid_f),
                                     &cat_time_refs[f]) != 0)
                    all_valid = 0;
            } else {
                all_valid = 0;
            }
        }

        if (all_valid) {
            for (int f = 1; f < nfiles; f++) {
                if (fabs(cat_time_refs[f].ref_jdn -
                         cat_time_refs[0].ref_jdn) > 1e-10 ||
                    fabs(cat_time_refs[f].step_days -
                         cat_time_refs[0].step_days) > 1e-15) {
                    cat_need_convert = 1;
                    break;
                }
            }
        }
    }

    /* Build output dataset: same as first but with concatenated time dim */
    ked_dataset_t *out = ked_calloc(1, sizeof(ked_dataset_t));
    out->format = KED_FMT_NC4;
    out->ndims = first->ndims;
    out->dims = ked_calloc((size_t)out->ndims, sizeof(ked_dim_t));
    memcpy(out->dims, first->dims, (size_t)out->ndims * sizeof(ked_dim_t));
    out->dims[time_did].len = total_time;

    out->nvars = first->nvars;
    out->vars = ked_calloc((size_t)out->nvars, sizeof(ked_var_t));
    for (int v = 0; v < out->nvars; v++) {
        out->vars[v] = first->vars[v];
        out->vars[v].atts = copy_attrs(first->vars[v].atts, first->vars[v].natts);
        /* Update shape for time dimension */
        for (int d = 0; d < out->vars[v].ndims; d++) {
            if (out->vars[v].dimids[d] == time_did)
                out->vars[v].shape[d] = total_time;
        }
    }

    out->ngatts = first->ngatts;
    out->gatts = copy_attrs(first->gatts, first->ngatts);
    apply_compress(out, compress);

    /* Create output */
    if (ked_nc_create(out, outpath) != 0) {
        for (int f = 0; f < nfiles; f++) ked_dataset_close(inputs[f]);
        free(inputs);
        free(cat_time_refs);
        ked_dataset_close(out);
        return 1;
    }

    /* For each variable, concatenate data along time */
    int ncid = *(int *)out->backend;

    for (int v = 0; v < first->nvars; v++) {
        ked_var_t *var = &first->vars[v];

        /* Check if this variable uses the time dimension */
        int time_axis = -1;
        for (int d = 0; d < var->ndims; d++) {
            if (var->dimids[d] == time_did) {
                time_axis = d;
                break;
            }
        }

        if (time_axis < 0) {
            /* No time dim — just copy from first file */
            size_t sz = ked_var_size(var);
            if (sz == 0) continue;
            void *buf = ked_malloc(sz);
            if (ked_dataset_read_var(first, v, buf) == 0)
                ked_nc_write_var(out, v, buf);
            free(buf);
            continue;
        }

        /* Variable has time dimension — concatenate from all files */
        size_t time_offset = 0;
        for (int f = 0; f < nfiles; f++) {
            /* Find matching var in this input */
            int sv = -1;
            for (int iv = 0; iv < inputs[f]->nvars; iv++) {
                if (strcmp(inputs[f]->vars[iv].name, var->name) == 0) {
                    sv = iv;
                    break;
                }
            }
            if (sv < 0) continue;

            ked_var_t *svar = &inputs[f]->vars[sv];
            size_t sz = ked_var_size(svar);
            if (sz == 0) { continue; }

            void *buf = ked_malloc(sz);
            if (ked_dataset_read_var(inputs[f], sv, buf) != 0) {
                free(buf);
                continue;
            }

            /* Convert time coordinate values if needed */
            if (cat_need_convert && v == time_coord_vidx && f > 0) {
                convert_time_buf(buf, svar->shape[time_axis], svar->type,
                                 &cat_time_refs[f], &cat_time_refs[0]);
            }

            /* Write at correct time offset using nc_put_vara */
            size_t start[KED_MAX_DIMS] = {0};
            size_t count[KED_MAX_DIMS] = {0};
            for (int d = 0; d < svar->ndims; d++) {
                if (d == time_axis) {
                    start[d] = time_offset;
                    count[d] = svar->shape[d];
                } else {
                    start[d] = 0;
                    count[d] = svar->shape[d];
                }
            }

            nc_put_vara(ncid, out->vars[v].varid, start, count, buf);
            free(buf);

            /* Find time dim length in this input */
            int tdid = find_dim_by_name(inputs[f], first->dims[time_did].name);
            time_offset += inputs[f]->dims[tdid].len;
        }
    }

    for (int f = 0; f < nfiles; f++) ked_dataset_close(inputs[f]);
    free(inputs);
    free(cat_time_refs);
    ked_dataset_close(out);
    return 0;
}
