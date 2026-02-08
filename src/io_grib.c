#include "io_grib.h"
#include "util.h"

#include <eccodes.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum number of distinct fields/levels/timesteps we handle */
#define MAX_FIELDS   256
#define MAX_LEVELS   256
#define MAX_TIMES    4096

/* Key for identifying a unique variable: shortName + typeOfLevel */
typedef struct {
    char   short_name[KED_MAX_NAME];
    char   type_of_level[KED_MAX_NAME];
    char   name[KED_MAX_NAME];       /* long name from GRIB */
    char   units[KED_MAX_NAME];
    int    nbit;                     /* bitsPerValue */
    long   grid_ni;
    long   grid_nj;
    char   grid_type[64];
    double levels[MAX_LEVELS];       /* levels seen for THIS field */
    int    nlevels;
} grib_field_t;

/* Timestep entry */
typedef struct {
    long date;   /* dataDate: YYYYMMDD */
    long time;   /* dataTime: HHMM */
} grib_time_t;

/* Scan context: accumulated info from all messages */
typedef struct {
    grib_field_t fields[MAX_FIELDS];
    int          nfields;
    grib_time_t  times[MAX_TIMES];
    int          ntimes;
    long         edition;
    long         grid_ni;
    long         grid_nj;
    long         nvalues;        /* numberOfValues per message (for irregular grids) */
    double       lat_first, lat_last;
    double       lon_first, lon_last;
    char         grid_type[64];
    int          nmessages;
} grib_scan_t;

/* Find or add a field. Returns index. */
static int find_or_add_field(grib_scan_t *scan, const char *sn,
                             const char *tol)
{
    for (int i = 0; i < scan->nfields; i++) {
        if (strcmp(scan->fields[i].short_name, sn) == 0 &&
            strcmp(scan->fields[i].type_of_level, tol) == 0)
            return i;
    }
    if (scan->nfields >= MAX_FIELDS) return -1;
    int idx = scan->nfields++;
    snprintf(scan->fields[idx].short_name, KED_MAX_NAME, "%s", sn);
    snprintf(scan->fields[idx].type_of_level, KED_MAX_NAME, "%s", tol);
    scan->fields[idx].nlevels = 0;
    return idx;
}

/* Find or add a level value within a field. Returns index. */
static int find_or_add_field_level(grib_field_t *fld, double level)
{
    for (int i = 0; i < fld->nlevels; i++) {
        if (fld->levels[i] == level) return i;
    }
    if (fld->nlevels >= MAX_LEVELS) return -1;
    fld->levels[fld->nlevels] = level;
    return fld->nlevels++;
}

/* Find or add a time entry. Returns index. */
static int find_or_add_time(grib_scan_t *scan, long date, long time)
{
    for (int i = 0; i < scan->ntimes; i++) {
        if (scan->times[i].date == date && scan->times[i].time == time)
            return i;
    }
    if (scan->ntimes >= MAX_TIMES) return -1;
    scan->times[scan->ntimes].date = date;
    scan->times[scan->ntimes].time = time;
    return scan->ntimes++;
}

/* Get a string key, falling back to empty string */
static void get_string(codes_handle *h, const char *key, char *buf, size_t sz)
{
    size_t len = sz;
    int err = codes_get_string(h, key, buf, &len);
    if (err != CODES_SUCCESS) buf[0] = '\0';
}

/* Get a long key, returning 0 on failure */
static long get_long(codes_handle *h, const char *key)
{
    long val = 0;
    codes_get_long(h, key, &val);
    return val;
}

/* Get a double key, returning 0.0 on failure */
static double get_double(codes_handle *h, const char *key)
{
    double val = 0.0;
    codes_get_double(h, key, &val);
    return val;
}

/* Scan all GRIB messages in a file to build inventory */
static int scan_grib_file(const char *path, grib_scan_t *scan)
{
    memset(scan, 0, sizeof(*scan));

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ked: cannot open '%s': %s\n", path, strerror(errno));
        return -1;
    }

    int err = 0;
    codes_handle *h = NULL;

    while ((h = codes_handle_new_from_file(NULL, f, PRODUCT_GRIB, &err)) != NULL) {
        scan->nmessages++;

        /* Edition (take from first message) */
        if (scan->nmessages == 1) {
            scan->edition = get_long(h, "edition");
        }

        /* Field identity */
        char sn[KED_MAX_NAME], tol[KED_MAX_NAME];
        get_string(h, "shortName", sn, sizeof(sn));
        get_string(h, "typeOfLevel", tol, sizeof(tol));

        int fi = find_or_add_field(scan, sn, tol);
        if (fi >= 0) {
            grib_field_t *fld = &scan->fields[fi];
            if (fld->name[0] == '\0') {
                get_string(h, "name", fld->name, sizeof(fld->name));
            }
            if (fld->units[0] == '\0') {
                get_string(h, "units", fld->units, sizeof(fld->units));
            }
            fld->nbit = (int)get_long(h, "bitsPerValue");
            fld->grid_ni = get_long(h, "Ni");
            fld->grid_nj = get_long(h, "Nj");
            get_string(h, "gridType", fld->grid_type, sizeof(fld->grid_type));

            /* Track level per field */
            double level = get_double(h, "level");
            find_or_add_field_level(fld, level);
        }

        /* Time */
        long date = get_long(h, "dataDate");
        long time = get_long(h, "dataTime");
        find_or_add_time(scan, date, time);

        /* Grid info (from first message) */
        if (scan->nmessages == 1) {
            scan->grid_ni = get_long(h, "Ni");
            scan->grid_nj = get_long(h, "Nj");
            scan->nvalues = get_long(h, "numberOfValues");
            scan->lat_first = get_double(h, "latitudeOfFirstGridPointInDegrees");
            scan->lat_last = get_double(h, "latitudeOfLastGridPointInDegrees");
            scan->lon_first = get_double(h, "longitudeOfFirstGridPointInDegrees");
            scan->lon_last = get_double(h, "longitudeOfLastGridPointInDegrees");
            get_string(h, "gridType", scan->grid_type, sizeof(scan->grid_type));
        }

        codes_handle_delete(h);
    }

    fclose(f);

    if (scan->nmessages == 0) {
        fprintf(stderr, "ked: no GRIB messages found in '%s'\n", path);
        return -1;
    }

    return 0;
}

/* Build ked attributes for a GRIB variable */
static ked_attr_t *make_grib_var_attrs(const grib_field_t *fld, int *natts_out)
{
    int natts = 0;
    ked_attr_t *atts = ked_calloc(8, sizeof(ked_attr_t));

    if (fld->name[0] != '\0') {
        snprintf(atts[natts].name, KED_MAX_NAME, "long_name");
        atts[natts].type = KED_TYPE_STRING;
        atts[natts].len = 1;
        atts[natts].value_str = strdup(fld->name);
        natts++;
    }
    if (fld->units[0] != '\0') {
        snprintf(atts[natts].name, KED_MAX_NAME, "units");
        atts[natts].type = KED_TYPE_STRING;
        atts[natts].len = 1;
        atts[natts].value_str = strdup(fld->units);
        natts++;
    }
    if (fld->type_of_level[0] != '\0') {
        snprintf(atts[natts].name, KED_MAX_NAME, "typeOfLevel");
        atts[natts].type = KED_TYPE_STRING;
        atts[natts].len = 1;
        atts[natts].value_str = strdup(fld->type_of_level);
        natts++;
    }
    if (fld->grid_type[0] != '\0') {
        snprintf(atts[natts].name, KED_MAX_NAME, "gridType");
        atts[natts].type = KED_TYPE_STRING;
        atts[natts].len = 1;
        atts[natts].value_str = strdup(fld->grid_type);
        natts++;
    }
    {
        snprintf(atts[natts].name, KED_MAX_NAME, "bitsPerValue");
        atts[natts].type = KED_TYPE_INT;
        atts[natts].len = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", fld->nbit);
        atts[natts].value_str = strdup(buf);
        natts++;
    }

    *natts_out = natts;
    return atts;
}

/* Build global attributes for GRIB dataset */
static ked_attr_t *make_grib_global_attrs(const grib_scan_t *scan,
                                          int *ngatts_out)
{
    int ngatts = 0;
    ked_attr_t *gatts = ked_calloc(8, sizeof(ked_attr_t));

    {
        snprintf(gatts[ngatts].name, KED_MAX_NAME, "format");
        gatts[ngatts].type = KED_TYPE_STRING;
        gatts[ngatts].len = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "GRIB edition %ld", scan->edition);
        gatts[ngatts].value_str = strdup(buf);
        ngatts++;
    }
    {
        snprintf(gatts[ngatts].name, KED_MAX_NAME, "messages");
        gatts[ngatts].type = KED_TYPE_INT;
        gatts[ngatts].len = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", scan->nmessages);
        gatts[ngatts].value_str = strdup(buf);
        ngatts++;
    }
    if (scan->grid_type[0] != '\0') {
        snprintf(gatts[ngatts].name, KED_MAX_NAME, "gridType");
        gatts[ngatts].type = KED_TYPE_STRING;
        gatts[ngatts].len = 1;
        gatts[ngatts].value_str = strdup(scan->grid_type);
        ngatts++;
    }

    *ngatts_out = ngatts;
    return gatts;
}

/* Check if any field has multiple levels */
static int max_field_levels(const grib_scan_t *scan)
{
    int max = 0;
    for (int i = 0; i < scan->nfields; i++) {
        if (scan->fields[i].nlevels > max)
            max = scan->fields[i].nlevels;
    }
    return max;
}

ked_dataset_t *ked_grib_open(const char *path)
{
    grib_scan_t scan;
    if (scan_grib_file(path, &scan) != 0)
        return NULL;

    ked_dataset_t *ds = ked_calloc(1, sizeof(ked_dataset_t));
    snprintf(ds->path, sizeof(ds->path), "%s", path);
    ds->format = (scan.edition == 2) ? KED_FMT_GRIB2 : KED_FMT_GRIB1;
    ds->backend = NULL;
    ds->backend_close = NULL;

    /*
     * Build dimensions.
     * Regular grids: time, [level], lat, lon
     * Irregular grids (reduced_gg etc.): time, [level], ncells
     * Level dimension only created if any field has >1 level.
     */
    int ndims = 0;
    int time_dimid = -1, level_dimid = -1;
    int lat_dimid = -1, lon_dimid = -1, ncells_dimid = -1;
    int ml = max_field_levels(&scan);

    /* Detect irregular grids: Ni is MISSING (2147483647) */
    int irregular = (scan.grid_ni <= 0 || scan.grid_ni == 2147483647L);

    int max_dims = 5;
    ds->dims = ked_calloc((size_t)max_dims, sizeof(ked_dim_t));

    /* Time dimension */
    if (scan.ntimes > 0) {
        time_dimid = ndims;
        snprintf(ds->dims[ndims].name, KED_MAX_NAME, "time");
        ds->dims[ndims].dimid = ndims;
        ds->dims[ndims].len = (size_t)scan.ntimes;
        ds->dims[ndims].unlimited = true;
        ndims++;
    }

    /* Level dimension (only if any field has >1 level) */
    if (ml > 1) {
        level_dimid = ndims;
        snprintf(ds->dims[ndims].name, KED_MAX_NAME, "level");
        ds->dims[ndims].dimid = ndims;
        ds->dims[ndims].len = (size_t)ml;
        ds->dims[ndims].unlimited = false;
        ndims++;
    }

    if (irregular) {
        /* Irregular grid: single ncells dimension */
        if (scan.nvalues > 0) {
            ncells_dimid = ndims;
            snprintf(ds->dims[ndims].name, KED_MAX_NAME, "ncells");
            ds->dims[ndims].dimid = ndims;
            ds->dims[ndims].len = (size_t)scan.nvalues;
            ds->dims[ndims].unlimited = false;
            ndims++;
        }
    } else {
        /* Regular grid: lat x lon */
        if (scan.grid_nj > 0) {
            lat_dimid = ndims;
            snprintf(ds->dims[ndims].name, KED_MAX_NAME, "lat");
            ds->dims[ndims].dimid = ndims;
            ds->dims[ndims].len = (size_t)scan.grid_nj;
            ds->dims[ndims].unlimited = false;
            ndims++;
        }

        if (scan.grid_ni > 0) {
            lon_dimid = ndims;
            snprintf(ds->dims[ndims].name, KED_MAX_NAME, "lon");
            ds->dims[ndims].dimid = ndims;
            ds->dims[ndims].len = (size_t)scan.grid_ni;
            ds->dims[ndims].unlimited = false;
            ndims++;
        }
    }

    ds->ndims = ndims;

    /*
     * Build variables.
     * Each unique shortName+typeOfLevel -> one variable.
     * Shape depends on per-field level count:
     *   - if field has >1 level: time x level x lat x lon
     *   - if field has 1 level: time x lat x lon
     */
    ds->nvars = scan.nfields;
    ds->vars = ked_calloc((size_t)ds->nvars, sizeof(ked_var_t));

    for (int v = 0; v < scan.nfields; v++) {
        grib_field_t *fld = &scan.fields[v];
        ked_var_t *var = &ds->vars[v];

        snprintf(var->name, KED_MAX_NAME, "%s", fld->short_name);
        var->varid = v;
        var->type = KED_TYPE_FLOAT;

        /* Build dimension list */
        int di = 0;
        if (time_dimid >= 0) {
            var->dimids[di] = time_dimid;
            var->shape[di] = ds->dims[time_dimid].len;
            di++;
        }
        /* Only add level dim if this field has multiple levels */
        if (level_dimid >= 0 && fld->nlevels > 1) {
            var->dimids[di] = level_dimid;
            var->shape[di] = (size_t)fld->nlevels;
            di++;
        }
        if (ncells_dimid >= 0) {
            var->dimids[di] = ncells_dimid;
            var->shape[di] = ds->dims[ncells_dimid].len;
            di++;
        }
        if (lat_dimid >= 0) {
            var->dimids[di] = lat_dimid;
            var->shape[di] = ds->dims[lat_dimid].len;
            di++;
        }
        if (lon_dimid >= 0) {
            var->dimids[di] = lon_dimid;
            var->shape[di] = ds->dims[lon_dimid].len;
            di++;
        }
        var->ndims = di;

        /* Variable attributes */
        var->atts = make_grib_var_attrs(fld, &var->natts);
    }

    /* Global attributes */
    ds->gatts = make_grib_global_attrs(&scan, &ds->ngatts);

    return ds;
}

int ked_grib_read_var(const ked_dataset_t *ds, int varidx, void *buf)
{
    const ked_var_t *var = &ds->vars[varidx];
    size_t nelems = ked_var_nelems(var);
    float *fbuf = (float *)buf;

    /* Initialize to zero */
    for (size_t i = 0; i < nelems; i++)
        fbuf[i] = 0.0f;

    FILE *f = fopen(ds->path, "rb");
    if (!f) {
        fprintf(stderr, "ked: cannot reopen '%s'\n", ds->path);
        return -1;
    }

    /* We need to figure out the grid size per message */
    size_t grid_size = 1;
    for (int d = 0; d < var->ndims; d++) {
        int did = var->dimids[d];
        /* Skip time and level dims — those index messages */
        if (strcmp(ds->dims[did].name, "time") != 0 &&
            strcmp(ds->dims[did].name, "level") != 0) {
            grid_size *= var->shape[d];
        }
    }

    /* Collect unique times from scan for ordering */
    grib_time_t times[MAX_TIMES];
    int ntimes = 0;

    /* First pass: collect unique times in order */
    int err = 0;
    codes_handle *h = NULL;
    FILE *f2 = fopen(ds->path, "rb");
    if (f2) {
        while ((h = codes_handle_new_from_file(NULL, f2, PRODUCT_GRIB, &err)) != NULL) {
            long date = get_long(h, "dataDate");
            long time_val = get_long(h, "dataTime");
            int found = 0;
            for (int i = 0; i < ntimes; i++) {
                if (times[i].date == date && times[i].time == time_val) {
                    found = 1;
                    break;
                }
            }
            if (!found && ntimes < MAX_TIMES) {
                times[ntimes].date = date;
                times[ntimes].time = time_val;
                ntimes++;
            }
            codes_handle_delete(h);
        }
        fclose(f2);
    }

    /* Second pass: read data for matching messages */
    double *tmp = malloc(grid_size * sizeof(double));
    if (!tmp) { fclose(f); return -1; }

    err = 0;
    while ((h = codes_handle_new_from_file(NULL, f, PRODUCT_GRIB, &err)) != NULL) {
        char sn[KED_MAX_NAME];
        get_string(h, "shortName", sn, sizeof(sn));

        if (strcmp(sn, var->name) != 0) {
            codes_handle_delete(h);
            continue;
        }

        /* Find time index */
        long date = get_long(h, "dataDate");
        long time_val = get_long(h, "dataTime");
        int tidx = -1;
        for (int i = 0; i < ntimes; i++) {
            if (times[i].date == date && times[i].time == time_val) {
                tidx = i;
                break;
            }
        }
        if (tidx < 0) { codes_handle_delete(h); continue; }

        /* Read values */
        size_t nvals = grid_size;
        int rc = codes_get_double_array(h, "values", tmp, &nvals);
        if (rc != CODES_SUCCESS || nvals != grid_size) {
            codes_handle_delete(h);
            continue;
        }

        /* Copy into output buffer at correct offset */
        size_t offset = (size_t)tidx * grid_size;
        for (size_t k = 0; k < grid_size; k++)
            fbuf[offset + k] = (float)tmp[k];

        codes_handle_delete(h);
    }

    free(tmp);
    fclose(f);
    return 0;
}
