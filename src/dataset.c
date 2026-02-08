#include "dataset.h"
#include "io_netcdf.h"

#ifdef KED_HAS_ECCODES
#include "io_grib.h"
#endif

#include <stdlib.h>
#include <string.h>

const char *ked_format_name(ked_format_t format)
{
    switch (format) {
    case KED_FMT_NC3:         return "netCDF-3 classic";
    case KED_FMT_NC3_64:      return "netCDF-3 64-bit offset";
    case KED_FMT_NC4:         return "netCDF-4";
    case KED_FMT_NC4_CLASSIC: return "netCDF-4 classic model";
    case KED_FMT_GRIB1:       return "GRIB edition 1";
    case KED_FMT_GRIB2:       return "GRIB edition 2";
    default:                  return "unknown";
    }
}

const char *ked_type_name(ked_type_t type)
{
    switch (type) {
    case KED_TYPE_BYTE:   return "byte";
    case KED_TYPE_CHAR:   return "char";
    case KED_TYPE_SHORT:  return "short";
    case KED_TYPE_INT:    return "int";
    case KED_TYPE_FLOAT:  return "float";
    case KED_TYPE_DOUBLE: return "double";
    case KED_TYPE_UBYTE:  return "ubyte";
    case KED_TYPE_USHORT: return "ushort";
    case KED_TYPE_UINT:   return "uint";
    case KED_TYPE_INT64:  return "int64";
    case KED_TYPE_UINT64: return "uint64";
    case KED_TYPE_STRING: return "string";
    default:              return "unknown";
    }
}

size_t ked_var_nelems(const ked_var_t *var)
{
    if (var->ndims == 0) return 1; /* scalar */
    size_t n = 1;
    for (int i = 0; i < var->ndims; i++) {
        n *= var->shape[i];
    }
    return n;
}

size_t ked_type_size(ked_type_t type)
{
    switch (type) {
    case KED_TYPE_BYTE: case KED_TYPE_CHAR: case KED_TYPE_UBYTE:
        return 1;
    case KED_TYPE_SHORT: case KED_TYPE_USHORT:
        return 2;
    case KED_TYPE_INT: case KED_TYPE_UINT: case KED_TYPE_FLOAT:
        return 4;
    case KED_TYPE_DOUBLE: case KED_TYPE_INT64: case KED_TYPE_UINT64:
        return 8;
    default:
        return 0;
    }
}

size_t ked_var_size(const ked_var_t *var)
{
    return ked_var_nelems(var) * ked_type_size(var->type);
}

int ked_dataset_read_var(const ked_dataset_t *ds, int varidx, void *buf)
{
    if (varidx < 0 || varidx >= ds->nvars) return -1;

    switch (ds->format) {
    case KED_FMT_NC3:
    case KED_FMT_NC3_64:
    case KED_FMT_NC4:
    case KED_FMT_NC4_CLASSIC:
        return ked_nc_read_var(ds, varidx, buf);

#ifdef KED_HAS_ECCODES
    case KED_FMT_GRIB1:
    case KED_FMT_GRIB2:
        return ked_grib_read_var(ds, varidx, buf);
#endif

    default:
        return -1;
    }
}

void ked_dataset_close(ked_dataset_t *ds)
{
    if (!ds) return;
    if (ds->backend_close) ds->backend_close(ds->backend);
    for (int v = 0; v < ds->nvars; v++) {
        if (ds->vars[v].atts) {
            for (int a = 0; a < ds->vars[v].natts; a++) {
                free(ds->vars[v].atts[a].value_str);
                free(ds->vars[v].atts[a].value_raw);
            }
            free(ds->vars[v].atts);
        }
    }
    free(ds->vars);
    free(ds->dims);
    if (ds->gatts) {
        for (int a = 0; a < ds->ngatts; a++) {
            free(ds->gatts[a].value_str);
            free(ds->gatts[a].value_raw);
        }
        free(ds->gatts);
    }
    free(ds);
}
