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
  * @brief String utils
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/string.c
  */

#include "ldb.h"
#include "ldb_string.h"
/**
 * @brief Verifies if a buffer contains printable characters
 * 
 * @param str String to verify
 * @return true String contains printable characters, false otherwise.
 */

bool ldb_valid_ascii(char *str)
{
	if (strlen(str) < 1) return false;
	for (int i = 0; i < strlen(str); i++) 
		if (str[i] < 33 || str[i] > 126) return false;
	return true;
}

/**
 * @brief remove whitespace characters from the start and end of a string
 * 
 * @param str String to work on.
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
 * @brief Divides a string into two when the first occurrence of a delimiter is found.
 * In the position where the delimiter was found, a null character is added. This means that the first part of the string is in the input buffer.
 * To obtain the position of the second part use the return value.
 * 
 * @param string The string to be split.
 * @param separator The delimiter to split the string on.
 * @return int Pointer to the second part of the string.
 */
int ldb_split_string(char *string, char separator)
{
	int pos;
	for (pos = 0; pos < strlen(string); pos++) if (string[pos] == separator) break;
	string[pos] = 0;
	return pos + 1;
}

/**
 * @brief Verifies if a string is a valid dbname/tablename
 * To identificate a table, a string must have the following format: "dbname/tablename"
 * 
 * @param str String to verify
 * @return true valid dbname/tablename, false otherwise.
 */
bool ldb_valid_name(char *str)
{
	if (strlen(str) >= LDB_MAX_NAME) return false;
	if (strstr(str, "/")) return false;
	if (strstr(str, ".")) return false;
	return true;
}

/**
 * @brief Gives the length of the first word in a string.
 * It counts how many characters are before the first space.
 * 
 * @param text String to be searched.
 * @return int Characters found before the first space (Space not included)
 */
int ldb_word_len(char *text)
{
	for (int i=0; i<strlen(text); i++) if (text[i] == ' ') return i;
	return strlen(text);
}

/**
 * @brief Verify if a pair DBNAME/TABLENAME is valid and exists.
 *
 * @param table A string containing the dbname and table name. Must be in the form of "dbname/tablename".
 * @return true Name valid, db and table exists. False otherwise.
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
 * A word is considered to be a sequence of characters separated by spaces.
 * 
 * @param text String to be searched.
 * @return int Return the number of words in the text. 
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
 * Example: ldb_extract_word(3, "This is a test"); returns "a"
 * 
 * @param n The word number to extract. Starts at 1.
 * @param wordlist A list of words separated by spaces.
 * @return char* Pointer to the nth word in the list.
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
