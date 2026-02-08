/*
 * ked - Klimadaten EDitor
 * A lean climate data processing tool.
 */
#ifndef KED_H
#define KED_H

#define KED_VERSION "0.3.0"
#define KED_NAME    "ked"

#define KED_MAX_NAME  256
#define KED_MAX_PATH  4096
#define KED_MAX_DIMS  32
#define KED_MAX_ATTRS 128

/* Backend-independent data type */
typedef enum {
    KED_TYPE_BYTE,
    KED_TYPE_CHAR,
    KED_TYPE_SHORT,
    KED_TYPE_INT,
    KED_TYPE_FLOAT,
    KED_TYPE_DOUBLE,
    KED_TYPE_UBYTE,
    KED_TYPE_USHORT,
    KED_TYPE_UINT,
    KED_TYPE_INT64,
    KED_TYPE_UINT64,
    KED_TYPE_STRING,
    KED_TYPE_UNKNOWN,
} ked_type_t;

/* File format */
typedef enum {
    KED_FMT_NC3,
    KED_FMT_NC3_64,
    KED_FMT_NC4,
    KED_FMT_NC4_CLASSIC,
    KED_FMT_GRIB1,
    KED_FMT_GRIB2,
    KED_FMT_UNKNOWN,
} ked_format_t;

#endif /* KED_H */
