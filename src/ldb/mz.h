#ifndef __MZ_LDB_H
#define __MZ_LDB_H

#include "../ldb.h"
#include "collate.h"

/* MZ  */
#define MZ_CACHE_SIZE 16384
#define MZ_FILES 65536
#define MZ_HEAD 18 // Head contains 14 bytes of the MD5 + 4 bytes for compressed SIZE
#define MZ_MD5 14
#define MZ_SIZE 4
#define MZ_MAX_FILE (4 * 1048576)

struct mz_cache_item
{
	uint16_t length;
	uint8_t data[MZ_CACHE_SIZE];
};

struct mz_job
{
	char path[LDB_MAX_PATH]; // Path to mz file
	uint8_t *mz;       // Pointer to entire mz file contents
	uint64_t mz_ln;    // MZ file length
	uint8_t mz_id[2];  // MZ file ID (first two bytes of MD5s)
	uint8_t *id;       // MZ record ID
	uint64_t ln;       // MZ record length
	char md5[33];      // MZ record hex ID (MD5)
	char *data;        // Pointer to uncompressed data
	uint64_t data_ln;  // Uncompressed data length
	uint8_t *zdata;    // Pointer to compressed data
	uint64_t zdata_ln; // Compressed data length
	void *ptr;         // Pointer to temporary data
	uint64_t ptr_ln;   // Temporary data length
	uint32_t dup_c;    // Duplicated counter
	uint32_t igl_c;    // Ignored counter
	uint32_t orp_c;    // Orphan file counter
	uint32_t exc_c;    // Excluded file counter
	uint32_t min_c;    // Under MIN_FILE_SIZE file counter
	bool check_only;   // Perform only an mz validation (without list output)
	bool dump_keys;    // Dump unique keys to STDOUT
	bool orphan_rm;    // Remove orphans
	uint8_t *key;      // File key to be printed via STDOUT (-k)
	uint8_t *xkeys;    // List of keys to be excluded in (-o/-O)ptimisation
	uint64_t xkeys_ln; // Length of xkeys
	void *licenses; // Array of known license identifiers
	int license_count;            // Number of known license identifiers
	bool key_found;			// Used with mz_key_exists
	void  (*decrypt) (uint8_t *data, uint32_t len);
};


bool mz_key_exists(struct mz_job *job, uint8_t *key);
bool mz_id_exists(uint8_t *mz, uint64_t size, uint8_t *id);
uint8_t *file_read(char *filename, uint64_t *size);

void mz_deflate(struct mz_job *job);
#define MZ_DEFLATE(job) mz_deflate(job)

void mz_id_fill(char *md5, uint8_t *mz_id);
void mz_parse(struct mz_job *job, bool (*mz_parse_handler) ());
void file_write(char *filename, uint8_t *src, uint64_t src_ln);
void mz_id_fill(char *md5, uint8_t *mz_id);
void mz_corrupted();
void mz_add(char *mined_path, uint8_t *md5, char *src, int src_ln, bool check, uint8_t *zsrc, struct mz_cache_item *mz_cache);
bool mz_check(char *path);
void mz_flush(char *mined_path, struct mz_cache_item *mz_cache);
void mz_list_check(struct mz_job *job);
void mz_list_keys(struct ldb_table table, int sector);
void mz_extract(struct mz_job *job);
void mz_cat(struct mz_job *job, char *key);
void ldb_mz_collate(struct ldb_table table, int p_sector);
void ldb_mz_collate_delete(struct ldb_table table, job_delete_tuples_t * delete);

#endif