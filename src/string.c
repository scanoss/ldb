// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/string.c
 *
 * String handling routines
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

 /**
  * @file string.c
  * @date 12 Jul 2020
  * @brief // TODO
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/string.c
  */

/**
 * @brief // TODO
 * 
 * @param str // TODO
 * @return true // TODO
 * @return false // TODO
 */
bool ldb_valid_ascii(char *str)
{
	if (strlen(str) < 1) return false;
	for (int i = 0; i < strlen(str); i++) 
		if (str[i] < 33 || str[i] > 126) return false;
	return true;
}

/**
 * @brief // TODO
 * 
 * @param str // TODO
 */
void ldb_trim(char *str)
{
	int i = 0;

	/* Left trim */
	int len = strlen(str);
	for (i = 0; i < len; i++) if (!isspace(str[i])) break;
	if (i) memmove(str, str + i, strlen(str + i) + 1);

	/* Right trim */
	len = strlen(str);
	for (i = len - 1; i >= 0 ; i--) if (!isspace(str[i])) break;
	str[i + 1] = 0;
}

/**
 * @brief // TODO
 * 
 * @param string // TODO
 * @param separator // TODO
 * @return int // TODO
 */
int ldb_split_string(char *string, char separator)
{
	int pos;
	for (pos = 0; pos < strlen(string); pos++) if (string[pos] == separator) break;
	string[pos] = 0;
	return pos + 1;
}

/**
 * @brief // TODO
 * 
 * @param str // TODO
 * @return true // TODO
 * @return false // TODO
 */
bool ldb_valid_name(char *str)
{
	if (strlen(str) >= LDB_MAX_NAME) return false;
	if (strstr(str, "/")) return false;
	if (strstr(str, ".")) return false;
	return true;
}

/**
 * @brief // TODO
 * 
 * @param key // TODO
 * @param subkey // TODO
 * @param subkey_ln // TODO
 * @param data // TODO
 * @param size // TODO
 * @param iteration // TODO
 * @param ptr // TODO
 * @return true // TODO
 * @return false // TODO
 */
bool ldb_asciiprint(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr)
{
	for (int i = 0; i < LDB_KEY_LN; i++) printf("%02x", key[i]);
	for (int i = 0; i < subkey_ln; i++)  printf("%02x", subkey[i]);

	printf(": ");

	for (int i = 0; i < size; i++)
		if (data[i] >= 32 && data[i] <= 126)
			fwrite(data + i, 1, 1, stdout);
		else
			fwrite(".", 1, 1, stdout);

	fwrite("\n", 1, 1, stdout);
	return false;
}

/**
 * @brief // TODO
 * 
 * @param key // TODO
 * @param subkey // TODO
 * @param subkey_ln // TODO
 * @param data // TODO
 * @param size // TODO
 * @param iteration // TODO
 * @param ptr // TODO
 * @return true // TODO
 * @return false // TODO
 */
bool ldb_csvprint(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr)
{
	/* Print key in hex (first CSV field) */
	for (int i = 0; i < LDB_KEY_LN; i++) printf("%02x", key[i]);
	for (int i = 0; i < subkey_ln; i++)  printf("%02x", subkey[i]);

	/* Print remaining hex bytes (if any, as a second CSV field) */
	int *hex_bytes = ptr;
	int remaining_hex = *hex_bytes - LDB_KEY_LN - subkey_ln;
	if (remaining_hex < 0) remaining_hex = 0;
	if (remaining_hex)
	{
		printf(",");
		for (int i = 0; i < remaining_hex; i++)  printf("%02x", data[i]);
	}

	/* Print remaining CSV data */
	printf(",");
	for (int i = remaining_hex; i < size; i++)
		if (data[i] >= 32 && data[i] <= 126)
			fwrite(data + i, 1, 1, stdout);
		else
			fwrite(".", 1, 1, stdout);

	fwrite("\n", 1, 1, stdout);
	return false;
}

/**
 * @brief // TODO
 * 
 * @param text // TODO
 * @return int // TODO
 */
int ldb_word_len(char *text)
{
	for (int i=0; i<strlen(text); i++) if (text[i] == ' ') return i;
	return strlen(text);
}

/**
 * @brief // TODO
 * 
 * @param table // TODO
 * @return true // TODO
 * @return false // TODO
 */
bool ldb_valid_table(char *table)
{

	// Make sure there is at least a byte before and after a slash
	int s = 0;
	int c = 0;
	for (int i = 0; i < strlen(table); i++)
	{
		if (table[i] == '/') 
		{
			c++;
			s = i;
		}
	}
	if (s < 1 || s > (strlen(table) - 1) || c != 1) 
	{
		printf("E060 Table name format should be dbname/tablename\n");
		return false;
	}

	// Verify that db/table path is not too long
	if (strlen(table) + strlen(ldb_root) + 1 >= LDB_MAX_PATH)
	{
		printf("E061 db/table name is too long\n");
		return false;
	}

	bool out = true;

	char *db_path    = malloc(LDB_MAX_PATH);
	sprintf(db_path, "%s/%s", ldb_root, table);
	db_path[strlen(ldb_root) + 1 + s] = 0;

	char *table_path    = malloc(LDB_MAX_PATH);
	sprintf(table_path, "%s/%s", ldb_root, table);

	// Verify that db exists

	if (!ldb_dir_exists(db_path)) 
	{
		printf("E062 Database %s does not exist\n", db_path + strlen(ldb_root) + 1);
		out = false;
	}

	// Verify that table exists
	else if (!ldb_dir_exists(table_path))
	{
		printf("E063 Table %s does not exist\n", table);
		out = false;
	}

	free(db_path);
	free(table_path);

	return out;
}


/**
 * @brief Counts number of words in normalized text
 * 
 * @param text // TODO
 * @return int // TODO
 */
int ldb_word_count(char *text)
{
	int words = 1;
	for (int i = 0; i < strlen(text); i++) if (text[i] == ' ') words++;
	return words;
}

/**
 * @brief Returns a pointer to a string containing the n word of the (normalized) list
 * 
 * @param n // TODO
 * @param wordlist // TODO
 * @return char* // TODO
 */
char *ldb_extract_word(int n, char *wordlist)
{
	int word_start = 0;
	char *out = calloc(LDB_MAX_COMMAND_SIZE + 1, 1);
	int limit = strlen(wordlist);
	if (limit > LDB_MAX_COMMAND_SIZE) limit = LDB_MAX_COMMAND_SIZE;

	// Look for word start
	if (n>1)
	{
		int c = 2;
		for (int i = 1; i < limit; i++)
		{
			if (wordlist[i] == ' ') 
			{
				if (c++ == n) 
				{
					word_start = i + 1;
					break;
				}
			}
		}
		if (word_start == 0) return out;
	}

	// Copy desired word to out
	int bytes = ldb_word_len(wordlist + word_start);
	if (bytes > LDB_MAX_COMMAND_SIZE) bytes = LDB_MAX_COMMAND_SIZE;

	memcpy(out, wordlist + word_start, bytes);
	return out;

}
