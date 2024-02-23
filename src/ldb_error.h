#ifndef LDB_ERROR_H
#define LDB_ERROR_H

#define LDB_ERROR_NOERROR 0
#define LDB_ERROR_DATA_RECORD_SIZE_EXCEED -53 //E053 Data record size exceeded
#define LDB_ERROR_DATA_SECTOR_CORRUPTED -56 // "E056 Data sector corrupted
#define LDB_ERROR_MAP_CORRUPTED -57//E057 Map location
#define LDB_ERROR_NODE_WRITE_FAILS -58//E058 Error writing node
#define LDB_ERROR_NODE_SIZE_INVALID -60 //E060 Unsupported node_length size (must be 2 or 4 bytes)
#define LDB_ERROR_RECORD_LENGHT_INVAID -76 //E076 Max record length should equal fixed record length
#define LDB_ERROR_CSV_WRONG_ENCODING -80 // E080 the csv file has an incorrect encoding

#define LDB_ERROR_MEM_NOMEM -200 //no memory available
#endif