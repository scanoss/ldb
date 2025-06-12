#ifndef __TYPES_H
#define __TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "definitions.h"
struct ldb_table
{
	char db[LDB_MAX_NAME];
	char table[LDB_MAX_NAME];
	int  key_ln;
	int  rec_ln; // data record length, otherwise 0 for variable-length data
    int  ts_ln;  // 2 or 4 (16-bit or 32-bit reserved for total sector size)
	bool tmp; // is this a .tmp sector instead of a .ldb?
	int keys;
	uint8_t *current_key;
	uint8_t *last_key;
	int definitions;	// Table definitions: is MZ? is encrypted? 
};

struct ldb_recordset
{
	char db[LDB_MAX_NAME];
	char table[LDB_MAX_NAME];
	FILE *sector;       // Data sector file pointer
	uint8_t key[255];   // Data key
	uint8_t key_ln;     // Key length: 4-255
	uint8_t subkey_ln;  // remaining part of the key that goes into the data: key_ln - 4
	uint8_t rec_ln;     // Fixed length of data records: 0-255, where 0 means variable-length data
	uint8_t *node;      // Pointer to current node. This will point to mallocated memory.
	uint32_t node_ln;   // Length of the current node
	uint8_t *record;    // Pointer to current record within node
	uint64_t next_node; // Location of next node inside the 
	uint64_t last_node; // Location of last node of the list
    uint8_t ts_ln;      // 2 or 4 (16-bit or 32-bit reserved for total sector size)
};
typedef struct ldb_sector_t
{
	uint8_t id;
	size_t size;
	uint8_t * data;
	FILE * file;
	bool failure;
} ldb_sector_t;

typedef bool (*ldb_record_handler) (uint8_t *, uint8_t *, int, uint8_t *, uint32_t, int, void *);



#endif