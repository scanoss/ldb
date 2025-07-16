// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/ldb.h
 *
 * LDB Database - A mapped linked-list database
 *
 * Copyright (C) 2018-2020 SCANOSS.COM
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _LDB_GLOBAL_
#define _LDB_GLOBAL_

#include "./ldb/definitions.h"
#include "./ldb/types.h"
#include "./ldb/mz.h"

#define LDB_VERSION "4.1.6"

#define LDB_TABLE_DEFINITION_UNDEFINED -1
#define LDB_TABLE_DEFINITION_STANDARD 0
#define LDB_TABLE_DEFINITION_ENCRYPTED 1
#define LDB_TABLE_DEFINITION_COMPRESSED 4
#define LDB_TABLE_DEFINITION_MZ 2

extern bool ldb_read_failure;

bool ldb_file_exists(char *path);
bool ldb_dir_exists(char *path);
bool ldb_locked();
void ldb_error (char *txt);
void ldb_prepare_dir(char *path);
void ldb_lock(char * db_table);
void ldb_unlock(char * db_table);
void ldb_create_sector(char *sector_path);
void ldb_uint40_write(FILE *ldb_sector, uint64_t value);
void ldb_uint32_write(FILE *ldb_sector, uint32_t value);
uint32_t ldb_uint32_read(FILE *ldb_sector);
uint64_t ldb_uint40_read(FILE *ldb_sector);
uint16_t ldb_uint16_read(FILE *ldb_sector);
uint16_t uint16_read(uint8_t *pointer);
void uint16_write(uint8_t *pointer, uint16_t value);
uint32_t uint32_read(uint8_t *pointer);
void uint32_write(uint8_t *pointer, uint32_t value);
uint64_t uint40_read(uint8_t *pointer);
void uint40_write(uint8_t *pointer, uint64_t value);
uint64_t ldb_map_pointer_pos(uint8_t *key);
uint64_t ldb_list_pointer(FILE *ldb_sector, uint8_t *key);
uint64_t ldb_last_node_pointer(FILE *ldb_sector, uint64_t list_pointer);
void ldb_update_list_pointers(FILE *ldb_sector, uint8_t *key, uint64_t list, uint64_t new_node);
int ldb_node_write (struct ldb_table table, FILE *ldb_sector, uint8_t *key, uint8_t *data, uint32_t dataln, uint16_t records);
uint64_t ldb_node_read (uint8_t *sector, struct ldb_table table, FILE *ldb_sector, uint64_t ptr, uint8_t *key, uint32_t *bytes_read, uint8_t **out, int max_node_size);
uint64_t ldb_node_read_v2(ldb_sector_t *sector, struct ldb_table table, uint64_t ptr, uint8_t *key, uint32_t *bytes_read, uint8_t **out, int max_node_size);
char *ldb_sector_path (struct ldb_table table, uint8_t *key, char *mode);
FILE *ldb_open (struct ldb_table table, uint8_t *key, char *mode);
bool ldb_close(FILE * sector);
int ldb_close_unlock(FILE *fp);
void ldb_node_unlink (struct ldb_table table, uint8_t *key);
void ldb_hexprint(uint8_t *data, uint32_t len, uint8_t width);
void ldb_hex_to_bin(char *hex, int hex_ln, uint8_t *out);
void ldb_bin_to_hex(uint8_t *bin, uint32_t len, char *out);
bool ldb_check_root();
struct ldb_table ldb_read_cfg(char *db_table);
void ldb_write_cfg(char *db, char *table, int keylen, int reclen, int keys, int definitions);
bool ldb_valid_table(char *table);
bool ldb_syntax_check(char *command, int *command_nr, int *word_nr);
void ldb_version(char **version);
bool ldb_database_exists(char *db);
bool ldb_table_exists(char *db, char*table);
bool ldb_create_table_new(char *db, char *table, int keylen, int reclen, int keys, int definitions);
bool ldb_create_table(char *db, char *table, int keylen, int reclen);
bool ldb_create_database(char *database);
struct ldb_recordset ldb_recordset_init(char *db, char *table, uint8_t *key);
void ldb_list_unlink(FILE *ldb_sector, uint8_t *key);
uint8_t *ldb_load_sector (struct ldb_table table, uint8_t *key);
ldb_sector_t ldb_load_sector_v2(struct ldb_table table, uint8_t *key);
bool ldb_validate_node(uint8_t *node, uint32_t node_size, int subkey_ln);
//bool uint32_is_zero(uint8_t *n);
bool ldb_key_exists(struct ldb_table table, uint8_t *key);
bool ldb_key_in_recordset(uint8_t *rs, uint32_t rs_len, uint8_t *subkey, uint8_t subkey_ln);
uint32_t ldb_fetch_recordset(uint8_t *sector, struct ldb_table table, uint8_t* key, bool skip_subkey, bool (*ldb_record_handler) (uint8_t *, uint8_t *, int, uint8_t *, uint32_t, int, void *), void *void_ptr);
uint32_t ldb_fetch_recordset_v2(ldb_sector_t * sector, struct ldb_table table, uint8_t* key, bool skip_subkey, bool (*ldb_record_handler) (uint8_t *, uint8_t *, int, uint8_t *, uint32_t, int, void *), void *void_ptr);
bool ldb_hexprint_width(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr);
void ldb_sector_update(struct ldb_table table, uint8_t *key);
void ldb_sector_erase(struct ldb_table table, uint8_t *key);
void ldb_dump(struct ldb_table table, int hex_bytes, int sector);
void ldb_dump_keys(struct ldb_table table, int s);

char * ldb_file_extension(char * path);
bool ldb_create_dir(char *path);
bool ldb_reverse_memcmp(uint8_t *a, uint8_t *b, int bytes);

void md5_string(const unsigned char *input, int len, unsigned char output[16]);
uint8_t * md5_file(char *path);

#define MD5(a, b, c)  md5_string(a, b, c)

#endif
