#ifndef KED_OPS_INFO_H
#define KED_OPS_INFO_H

/* Display detailed file info (dimensions, variables, attributes) */
int ked_op_info(const char *path);

/* Display short summary (one line per variable) */
int ked_op_sinfo(const char *path);

#endif /* KED_OPS_INFO_H */
