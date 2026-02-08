#ifndef KED_IO_GRIB_H
#define KED_IO_GRIB_H

#include "dataset.h"

/* Open a GRIB file and populate the dataset struct.
 * Returns NULL on error (prints message to stderr). */
ked_dataset_t *ked_grib_open(const char *path);

/* Read variable data into buf. buf must be ked_var_size() bytes.
 * Re-scans the GRIB file to extract data for the given variable.
 * Returns 0 on success, -1 on error. */
int ked_grib_read_var(const ked_dataset_t *ds, int varidx, void *buf);

#endif /* KED_IO_GRIB_H */
