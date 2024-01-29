#ifndef __DEFINITIONS_H
#define __DEFINITIONS_H

#define LDB_CFG_PATH "/usr/local/etc/scanoss/ldb/"
#define LDB_MAX_PATH 1024
#define LDB_MAX_NAME 64
#define LDB_MAX_RECORDS 500000 // Max number of records per list
#define LDB_MAX_REC_LN 65535
#define LDB_KEY_LN 4 // Main LDB key:  32-bit
#define LDB_PTR_LN 5 // Node pointers: 40-bit
#define LDB_MAP_SIZE (256 * 256 * 256 * 5) // Size of sector map
#define LDB_MAX_NODE_DATA_LN (4 * 1048576) // Maximum length for a data record in a node (4Mb)
#define LDB_MAX_NODE_LN ((256 * 256 * 18) - 1)
#define LDB_MAX_COMMAND_SIZE (64 * 1024)   // Maximum length for an LDB command statement
#define COLLATE_REPORT_SEC 5 // Report interval for collate status
#define MD5_LEN 16
#define MD5_LEN_HEX 32
#define BUFFER_SIZE 1048576
#define MAX_CSV_LINE_LEN 1024

extern char ldb_root[];
extern char ldb_lock_path[];
extern char *ldb_commands[];
extern int ldb_commands_count;
extern int ldb_cmp_width;

#endif