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

/* Key/hash calculation primitive (shared via libldb).
 * Lets a table compute its keys with MD5 (ldb_md5/md5_string) or CRC64 (ldb_crc64). */
typedef void (*hash_calc_t) (const unsigned char *input, int len, unsigned char * output);

typedef struct ldb_table
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
	hash_calc_t hash_calc;	// Hash primitive used to compute this table's keys (NULL = MD5)
} ldb_table_t;

/* In-memory/on-disk LDB sector handle.
 * If data is NULL the sector is read from disk through file (opened lazily);
 * otherwise data points to the whole sector loaded in RAM (size bytes).
 * failure is raised when a node read goes out of range. */
typedef struct ldb_sector_t
{
	uint8_t id;       // Sector id (first byte of the key)
	size_t size;      // Size of the in-memory sector (data), 0 when read from disk
	uint8_t *data;    // Whole sector loaded in RAM, NULL when read from disk
	FILE *file;       // Open file descriptor when reading from disk
	bool failure;     // Raised when a node pointer/size goes out of range
} ldb_sector_t;

/* Record handler: invoked for every record found by ldb_fetch_recordset.
 * Receives the table (so it can derive subkey_ln = table->key_ln - LDB_KEY_LN). */
typedef bool (*ldb_record_handler_t) (struct ldb_table *table, uint8_t *key, uint8_t *subkey, uint8_t *data, uint32_t data_len, int record_number, void *ptr);

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

#endif