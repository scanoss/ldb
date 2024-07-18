#ifndef _LDB_STRING_H
#define __LDB_STRING_H
#include "ldb.h"
bool ldb_asciiprint(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr);
bool ldb_csvprint(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr);
void ldb_trim(char *str);
int ldb_word_len(char *text);
int ldb_word_count(char *text);
bool ldb_valid_hex(char *str);
bool ldb_valid_ascii(char *str);
int ldb_split_string(char *string, char separator);
bool ldb_valid_name(char *str);
char *ldb_extract_word(int n, char *wordlist);
#endif