#include "io_netcdf.h"
#include "util.h"

#include <netcdf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Convert nc_type to ked_type_t */
static ked_type_t nc_to_ked_type(nc_type t)
{
    switch (t) {
    case NC_BYTE:   return KED_TYPE_BYTE;
    case NC_CHAR:   return KED_TYPE_CHAR;
    case NC_SHORT:  return KED_TYPE_SHORT;
    case NC_INT:    return KED_TYPE_INT;
    case NC_FLOAT:  return KED_TYPE_FLOAT;
    case NC_DOUBLE: return KED_TYPE_DOUBLE;
    case NC_UBYTE:  return KED_TYPE_UBYTE;
    case NC_USHORT: return KED_TYPE_USHORT;
    case NC_UINT:   return KED_TYPE_UINT;
    case NC_INT64:  return KED_TYPE_INT64;
    case NC_UINT64: return KED_TYPE_UINT64;
    case NC_STRING: return KED_TYPE_STRING;
    default:        return KED_TYPE_UNKNOWN;
    }
}

/* Convert NC_FORMAT_* to ked_format_t */
static ked_format_t nc_to_ked_format(int fmt)
{
    switch (fmt) {
    case NC_FORMAT_CLASSIC:         return KED_FMT_NC3;
    case NC_FORMAT_64BIT_OFFSET:    return KED_FMT_NC3_64;
    case NC_FORMAT_NETCDF4:         return KED_FMT_NC4;
    case NC_FORMAT_NETCDF4_CLASSIC: return KED_FMT_NC4_CLASSIC;
    default:                        return KED_FMT_UNKNOWN;
    }
}

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
        nc_type nc_t;
        nc_inq_attname(ncid, varid, i, atts[i].name);
        nc_inq_att(ncid, varid, atts[i].name, &nc_t, &atts[i].len);
        atts[i].type = nc_to_ked_type(nc_t);
        atts[i].value_str = format_attr_value(ncid, varid, atts[i].name,
                                              nc_t, atts[i].len);
    }
    return atts;
}

/* Check if path is a directory containing .zgroup or .zmetadata (Zarr store).
 * If so, build an NCZarr URI: file://<abspath>#mode=nczarr,zarr
 * Returns a malloc'd string the caller must free, or NULL if not Zarr. */
static char *make_zarr_uri(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
        return NULL;

    /* Check for Zarr v2 markers */
    char probe[KED_MAX_PATH];
    snprintf(probe, sizeof(probe), "%s/.zgroup", path);
    if (stat(probe, &st) != 0) {
        snprintf(probe, sizeof(probe), "%s/.zmetadata", path);
        if (stat(probe, &st) != 0)
            return NULL;
    }

    /* Resolve to absolute path */
    char *abs = realpath(path, NULL);
    if (!abs) return NULL;

    size_t len = strlen("file://") + strlen(abs) + strlen("#mode=nczarr,zarr") + 1;
    char *uri = ked_malloc(len);
    snprintf(uri, len, "file://%s#mode=nczarr,zarr", abs);
    free(abs);
    return uri;
}

/* Backend close: free ncid stored as heap int */
static void nc_backend_close(void *backend)
{
    if (!backend) return;
    int ncid = *(int *)backend;
    nc_close(ncid);
    free(backend);
}

ked_dataset_t *ked_nc_open(const char *path)
{
    /* Auto-detect Zarr directories and build NCZarr URI */
    char *zarr_uri = make_zarr_uri(path);
    const char *open_path = zarr_uri ? zarr_uri : path;

    int ncid;
    int rc = nc_open(open_path, NC_NOWRITE, &ncid);
    if (rc != NC_NOERR) {
        fprintf(stderr, "ked: cannot open '%s': %s\n", path, nc_strerror(rc));
        free(zarr_uri);
        return NULL;
    }

    ked_dataset_t *ds = ked_calloc(1, sizeof(ked_dataset_t));
    snprintf(ds->path, sizeof(ds->path), "%s", path);
    free(zarr_uri);

    /* Store ncid as backend handle */
    int *ncid_ptr = ked_malloc(sizeof(int));
    *ncid_ptr = ncid;
    ds->backend = ncid_ptr;
    ds->backend_close = nc_backend_close;

    /* Query format */
    int nc_fmt;
    nc_inq_format(ncid, &nc_fmt);
    ds->format = nc_to_ked_format(nc_fmt);

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
        nc_type nc_t;
        ds->vars[v].varid = v;
        nc_inq_var(ncid, v, ds->vars[v].name, &nc_t,
                   &ds->vars[v].ndims, ds->vars[v].dimids, &ds->vars[v].natts);
        ds->vars[v].type = nc_to_ked_type(nc_t);

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
