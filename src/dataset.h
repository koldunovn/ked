#ifndef KED_DATASET_H
#define KED_DATASET_H

#include "ked.h"

#include <stdbool.h>
#include <stddef.h>
#include <netcdf.h>

/* Attribute value (simplified - stores as string) */
typedef struct {
    char   name[KED_MAX_NAME];
    nc_type type;
    size_t  len;
    char   *value_str;  /* Human-readable string representation */
} ked_attr_t;

/* Variable */
typedef struct {
    char    name[KED_MAX_NAME];
    int     varid;
    int     ndims;
    int     dimids[KED_MAX_DIMS];
    nc_type type;
    size_t  shape[KED_MAX_DIMS];
    int     natts;
    ked_attr_t *atts;
} ked_var_t;

/* Dimension */
typedef struct {
    char   name[KED_MAX_NAME];
    int    dimid;
    size_t len;
    bool   unlimited;
} ked_dim_t;

/* Dataset */
typedef struct {
    char      path[KED_MAX_PATH];
    int       ncid;
    int       format;      /* NC_FORMAT_* constant */
    int       nvars;
    int       ndims;
    int       ngatts;      /* number of global attributes */
    ked_var_t *vars;
    ked_dim_t *dims;
    ked_attr_t *gatts;     /* global attributes */
} ked_dataset_t;

/* Open a dataset (netCDF or NCZarr). Returns NULL on error. */
ked_dataset_t *ked_dataset_open(const char *path);

/* Close and free a dataset */
void ked_dataset_close(ked_dataset_t *ds);

/* Get the format name as a string */
const char *ked_format_name(int format);

/* Get the type name as a string */
const char *ked_type_name(nc_type type);

/* Compute total number of elements in a variable */
size_t ked_var_nelems(const ked_var_t *var);

/* Compute variable data size in bytes */
size_t ked_var_size(const ked_var_t *var);

#endif /* KED_DATASET_H */
