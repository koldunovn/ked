#ifndef KED_IO_GRIB_H
#define KED_IO_GRIB_H

#include "dataset.h"

/* Open a GRIB file and populate the dataset struct.
 * Returns NULL on error (prints message to stderr). */
ked_dataset_t *ked_grib_open(const char *path);

#endif /* KED_IO_GRIB_H */
