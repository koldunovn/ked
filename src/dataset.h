#ifndef KED_DATASET_H
#define KED_DATASET_H

#include "ked.h"

#include <stdbool.h>
#include <stddef.h>

/* Attribute value */
typedef struct {
    char       name[KED_MAX_NAME];
    ked_type_t type;
    size_t     len;
    char      *value_str;  /* Human-readable string representation */
    void      *value_raw;  /* Raw binary data for type-preserving copy */
} ked_attr_t;

/* Variable */
typedef struct {
    char       name[KED_MAX_NAME];
    int        varid;
    int        ndims;
    int        dimids[KED_MAX_DIMS];
    ked_type_t type;
    size_t     shape[KED_MAX_DIMS];
    int        natts;
    ked_attr_t *atts;
    int        deflate;       /* 0=off, 1-9=deflate level */
    bool       shuffle;       /* shuffle filter */
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
    char         path[KED_MAX_PATH];
    ked_format_t format;
    int          nvars;
    int          ndims;
    int          ngatts;      /* number of global attributes */
    ked_var_t   *vars;
    ked_dim_t   *dims;
    ked_attr_t  *gatts;       /* global attributes */
    void        *backend;     /* backend-specific handle (opaque) */
    void       (*backend_close)(void *);
} ked_dataset_t;

/* Open a dataset (auto-detect format). Returns NULL on error. */
ked_dataset_t *ked_dataset_open(const char *path);

/* Close and free a dataset */
void ked_dataset_close(ked_dataset_t *ds);

/* Get the format name as a string */
const char *ked_format_name(ked_format_t format);

/* Get the type name as a string */
const char *ked_type_name(ked_type_t type);

/* Compute total number of elements in a variable */
size_t ked_var_nelems(const ked_var_t *var);

/* Compute variable data size in bytes */
size_t ked_var_size(const ked_var_t *var);

/* Get the size in bytes of a single element of a type */
size_t ked_type_size(ked_type_t type);

/* Read variable data into a caller-supplied buffer.
 * Buffer must be at least ked_var_size(var) bytes.
 * Returns 0 on success, -1 on error. */
int ked_dataset_read_var(const ked_dataset_t *ds, int varidx, void *buf);

#endif /* KED_DATASET_H */
