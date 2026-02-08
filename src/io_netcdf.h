#ifndef KED_IO_NETCDF_H
#define KED_IO_NETCDF_H

#include "dataset.h"

/* Open a netCDF/NCZarr file and populate the dataset struct.
 * Returns NULL on error (prints message to stderr). */
ked_dataset_t *ked_nc_open(const char *path);

/* Read variable data into buf. buf must be ked_var_size() bytes.
 * Returns 0 on success, -1 on error. */
int ked_nc_read_var(const ked_dataset_t *ds, int varidx, void *buf);

/* Create a new netCDF file from a dataset template.
 * Defines all dims, vars, and attributes. Leaves file in data mode.
 * Stores ncid in ds->backend. Returns 0 on success. */
int ked_nc_create(ked_dataset_t *ds, const char *path);

/* Write variable data from buf. buf must be ked_var_size() bytes.
 * Returns 0 on success, -1 on error. */
int ked_nc_write_var(const ked_dataset_t *ds, int varidx, const void *buf);

#endif /* KED_IO_NETCDF_H */
