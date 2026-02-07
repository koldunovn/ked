#ifndef KED_IO_NETCDF_H
#define KED_IO_NETCDF_H

#include "dataset.h"

/* Open a netCDF/NCZarr file and populate the dataset struct.
 * Returns NULL on error (prints message to stderr). */
ked_dataset_t *ked_nc_open(const char *path);

#endif /* KED_IO_NETCDF_H */
