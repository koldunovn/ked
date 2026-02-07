#include "dataset.h"
#include "io_netcdf.h"
#include "util.h"

#ifdef KED_HAS_ECCODES
#include "io_grib.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Detect file format from magic bytes.
 * Returns KED_FMT_UNKNOWN if unrecognized. */
static ked_format_t detect_format(const char *path)
{
    /* Check for Zarr directory first */
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        char probe[KED_MAX_PATH];
        snprintf(probe, sizeof(probe), "%s/.zgroup", path);
        if (stat(probe, &st) == 0) return KED_FMT_NC4; /* NCZarr */
        snprintf(probe, sizeof(probe), "%s/.zmetadata", path);
        if (stat(probe, &st) == 0) return KED_FMT_NC4; /* NCZarr */
    }

    FILE *f = fopen(path, "rb");
    if (!f) return KED_FMT_UNKNOWN;

    unsigned char magic[4];
    size_t n = fread(magic, 1, 4, f);
    fclose(f);

    if (n < 4) return KED_FMT_UNKNOWN;

    /* netCDF-3: "CDF\x01" or "CDF\x02" */
    if (magic[0] == 'C' && magic[1] == 'D' && magic[2] == 'F') {
        return KED_FMT_NC3;
    }

    /* HDF5 / netCDF-4: "\x89HDF" */
    if (magic[0] == 0x89 && magic[1] == 'H' && magic[2] == 'D' && magic[3] == 'F') {
        return KED_FMT_NC4;
    }

    /* GRIB: "GRIB" */
    if (magic[0] == 'G' && magic[1] == 'R' && magic[2] == 'I' && magic[3] == 'B') {
#ifdef KED_HAS_ECCODES
        return KED_FMT_GRIB1; /* exact edition determined by io_grib */
#else
        ked_die("GRIB file detected but ked was built without ecCodes support");
#endif
    }

    return KED_FMT_UNKNOWN;
}

ked_dataset_t *ked_dataset_open(const char *path)
{
    ked_format_t fmt = detect_format(path);

    switch (fmt) {
    case KED_FMT_NC3:
    case KED_FMT_NC3_64:
    case KED_FMT_NC4:
    case KED_FMT_NC4_CLASSIC:
        return ked_nc_open(path);

#ifdef KED_HAS_ECCODES
    case KED_FMT_GRIB1:
    case KED_FMT_GRIB2:
        return ked_grib_open(path);
#endif

    default:
        /* Fallback: try netCDF (handles edge cases like NCZarr URIs) */
        return ked_nc_open(path);
    }
}
