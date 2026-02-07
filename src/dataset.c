#include "dataset.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *ked_format_name(int format)
{
    switch (format) {
    case NC_FORMAT_CLASSIC:        return "netCDF-3 classic";
    case NC_FORMAT_64BIT_OFFSET:   return "netCDF-3 64-bit offset";
    case NC_FORMAT_NETCDF4:        return "netCDF-4";
    case NC_FORMAT_NETCDF4_CLASSIC:return "netCDF-4 classic model";
#ifdef NC_FORMAT_64BIT_DATA
    case NC_FORMAT_64BIT_DATA:     return "netCDF-3 64-bit data";
#endif
    default:                       return "unknown";
    }
}

const char *ked_type_name(nc_type type)
{
    switch (type) {
    case NC_BYTE:   return "byte";
    case NC_CHAR:   return "char";
    case NC_SHORT:  return "short";
    case NC_INT:    return "int";
    case NC_FLOAT:  return "float";
    case NC_DOUBLE: return "double";
    case NC_UBYTE:  return "ubyte";
    case NC_USHORT: return "ushort";
    case NC_UINT:   return "uint";
    case NC_INT64:  return "int64";
    case NC_UINT64: return "uint64";
    case NC_STRING: return "string";
    default:        return "unknown";
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

size_t ked_var_size(const ked_var_t *var)
{
    size_t elem_size = 0;
    switch (var->type) {
    case NC_BYTE: case NC_CHAR: case NC_UBYTE:
        elem_size = 1; break;
    case NC_SHORT: case NC_USHORT:
        elem_size = 2; break;
    case NC_INT: case NC_UINT: case NC_FLOAT:
        elem_size = 4; break;
    case NC_DOUBLE: case NC_INT64: case NC_UINT64:
        elem_size = 8; break;
    default:
        elem_size = 0; break;
    }
    return ked_var_nelems(var) * elem_size;
}
