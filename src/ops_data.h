#ifndef KED_OPS_DATA_H
#define KED_OPS_DATA_H

/* compress: -1=auto (preserve source), 0=off, 1-9=deflate level */

/* Copy input file to output file (format conversion) */
int ked_op_copy(const char *inpath, const char *outpath, int compress);

/* Select variables from input and write to output.
 * var_names is a comma-separated list of variable names. */
int ked_op_select(const char *inpath, const char *outpath,
                  const char *var_names, int compress);

/* Merge variables from multiple input files into one output.
 * nfiles inputs, last arg is output. */
int ked_op_merge(int nfiles, const char **inpaths, const char *outpath,
                 int compress);

/* Concatenate files along the time (unlimited) dimension. */
int ked_op_cat(int nfiles, const char **inpaths, const char *outpath,
               int compress);

#endif /* KED_OPS_DATA_H */
