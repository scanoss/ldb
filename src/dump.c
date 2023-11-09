// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/dump.c
 *
 * Dump command functions
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
#include "ldb.h"
#include <stdio.h>
#include "ldb_string.h"
 /**
  * @file dump.c
  * @date 12 Jul 2020 
  * @brief LDB dump functions
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/dump.c
  */

/**
 * @brief Dump LDB into stdout
 * 
 * @param table table name string
 * @param hex_bytes hex bytes format
 * @param sectorn sectur number
 */
void ldb_dump(struct ldb_table table, int hex_bytes, int sectorn)
{
	/* Read each DB sector */
	uint8_t k0 = 0;
	setlocale(LC_NUMERIC, "");

	if (sectorn >= 0) k0 = (uint8_t) sectorn;

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
						ldb_fetch_recordset(sector, table, k, true, ldb_csvprint, &hex_bytes);
					}
			free(sector);
		}
		if (sectorn >= 0) break;
	} while (k0++ < 255);
	fflush(stdout);
}
