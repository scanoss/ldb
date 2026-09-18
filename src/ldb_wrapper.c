// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/ldb_wrapper.c
 *
 * Shared-library entry point to query an LDB table by key. See ldb_wrapper.h.
 *
 * Copyright (C) 2018-2026 SCANOSS.COM
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

#include <stdlib.h>
#include <string.h>

#include "ldb.h"
#include "ldb_wrapper.h"
#include "logger.h"

/**
 * @brief Query an LDB table by key and collect every matching record.
 *
 * @param dbtable "<db>/<table>" to query (owned by the caller; not freed here).
 * @param key     lookup key in hex (>= 32 bits, up to the table's key_ln).
 * @return a heap-allocated T_RawRes with the concatenated records, each framed
 *         as [uint32 size][payload]; NULL on invalid input or allocation
 *         failure. The caller owns the result and must free result->data and
 *         result.
 */
T_RawRes *ldb_query_raw(char *dbtable, char *key)
{
	if (!dbtable || !key)
		return NULL;

	if (!ldb_valid_table(dbtable))
		return NULL;

	if (strlen(key) < 8)
	{
		log_info("E071 Key length cannot be less than 32 bits\n");
		return NULL;
	}

	int key_ln = (int) strlen(key) / 2;

	// Assemble the ldb table structure from its .cfg (does not modify dbtable).
	struct ldb_table ldbtable = ldb_read_cfg(dbtable);

	// The key must match the table's key_ln (or the 32-bit main LDB key).
	if (key_ln != ldbtable.key_ln && key_ln != LDB_KEY_LN)
	{
		log_info("E073 Provided key length is invalid\n");
		return NULL;
	}

	uint8_t *keybin = malloc(key_ln);
	if (!keybin)
	{
		log_info("E072 ldb_query_raw: out of memory allocating key buffer\n");
		return NULL;
	}
	ldb_hex_to_bin(key, strlen(key), keybin);

	T_RawRes *results = malloc(sizeof(T_RawRes));
	if (!results)
	{
		log_info("E072 ldb_query_raw: out of memory allocating result\n");
		free(keybin);
		return NULL;
	}
	results->data = malloc(LDB_MAX_NODE_DATA_LN);
	if (!results->data)
	{
		log_info("E072 ldb_query_raw: out of memory allocating result buffer\n");
		free(results);
		free(keybin);
		return NULL;
	}
	results->size = 0;
	results->capacity = LDB_MAX_NODE_DATA_LN;

	ldb_fetch_recordset(NULL, ldbtable, keybin, false, ldb_dump_row, results);

	free(keybin);
	return results;
}

/**
 * @brief Record handler for ldb_query_raw.
 * @details Appends the record to the T_RawRes in `ptr`, framed as
 *          [uint32 size][payload], growing the buffer in chunks as needed.
 *          Returns false to keep collecting; returns true to stop iteration
 *          when the buffer cannot be grown (the caller then gets a partial
 *          result) rather than aborting the host process.
 */
bool ldb_dump_row(struct ldb_table *table, uint8_t *key, uint8_t *subkey, uint8_t *data, uint32_t size, int record_number, void *ptr)
{
	(void) table;
	(void) key;
	(void) subkey;
	(void) record_number;

	T_RawRes *r = ptr;

	// Account for the 4-byte size prefix written alongside the payload.
	if (r->size + size + 4 > r->capacity)
	{
		size_t new_capacity = r->capacity + 2 * LDB_MAX_NODE_DATA_LN;
		while (r->size + size + 4 > new_capacity)
			new_capacity += 2 * LDB_MAX_NODE_DATA_LN;

		uint8_t *new_data = realloc(r->data, new_capacity);
		if (!new_data)
		{
			log_info("E074 ldb_query_raw: out of memory growing result buffer; returning partial result\n");
			return true; // stop iteration gracefully
		}
		r->data = new_data;
		r->capacity = new_capacity;
	}

	memcpy(&r->data[r->size], &size, 4);
	memcpy(&r->data[r->size + 4], data, size);
	r->size += size + 4;

	return false;
}
