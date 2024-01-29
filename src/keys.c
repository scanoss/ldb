// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/keys.c
 *
 * "Dump keys" command functions
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
  * @file keys.c
  * @date 24 Dec 2020 
  * @brief Handle the dump keys record: Write in stdout unique keys
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/keys.c
  */
#include "ldb.h"
/**
 * @brief Handle the dump keys record: Write in stdout unique keys
 * 
 * @param key block key
 * @param subkey block subkey
 * @param subkey_ln block subkey lenght
 * @param data uint8_t pointer to data to be added
 * @param size data size
 * @param iteration number of iterations
 * @param ptr[out] output pointer
 * @return true to finish the fetch
 * @return false to continue the fetch
 */
bool ldb_dump_keys_handler(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr)
{
	struct ldb_table *table = ptr;

	/* Assemble full key */
	memcpy(table->current_key, key, LDB_KEY_LN);
	memcpy(table->current_key + LDB_KEY_LN, subkey, subkey_ln);

	/* Skip if same as last key */
	if (ldb_reverse_memcmp(table->current_key, table->last_key, table->key_ln)) return false;

	/* Save into last key */
	memcpy(table->last_key, table->current_key, table->key_ln);

	char hex[MD5_LEN * 2 + 1];
	ldb_bin_to_hex(table->current_key, MD5_LEN, hex);
	printf("%s\n", hex);
	return false;
}

/**
 * @brief LDB dump keys trought stdout
 * 
 * @param table input table
 */
void ldb_dump_keys(struct ldb_table table, int s)
{
	/* Read each DB sector */
	uint8_t k0 = s >= 0 ? s : 0;
	setlocale(LC_NUMERIC, "");

	table.current_key = calloc(table.key_ln, 1);
	table.last_key = calloc(table.key_ln, 1);

	do {
		uint8_t *sector = ldb_load_sector(table, &k0);
		if (sector)
		{
			/* Read each one of the (256 ^ 3) list pointers from the map */
			uint8_t k[LDB_KEY_LN];
			k[0] = k0;
			for (int k1 = 0; k1 < 256; k1++)
				for (int k2 = 0; k2 < 256; k2++)
					for (int k3 = 0; k3 < 256; k3++)
					{
						k[1] = k1;
						k[2] = k2;
						k[3] = k3;
						
						/* Process records */
						ldb_fetch_recordset(sector, table, k, true, ldb_dump_keys_handler, &table);
						
					}
			free(sector);
			if (s >=0)
				break;
		}
	} while (k0++ < 255);

	fflush(stdout);

	free(table.current_key);
	free(table.last_key);
}
