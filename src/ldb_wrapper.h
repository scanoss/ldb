// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/ldb_wrapper.h
 *
 * Shared-library entry point to query an LDB table by key from a third-party
 * process (the historical single-key `ldb_query_raw` entry point, restored on
 * the new_hash / crc64 architecture).
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

#ifndef _LDB_WRAPPER_
#define _LDB_WRAPPER_

#include <stdbool.h>
#include <stdint.h>

#include "./ldb/definitions.h"
#include "./ldb/types.h"
#include "./ldb/mz.h"

/* Concatenated recordset returned by ldb_query_raw. `data` holds every matching
 * record framed as [uint32 size][payload]; `size` is the used length and
 * `capacity` the allocation. */
typedef struct {
	uint32_t size;
	uint32_t capacity;
	uint8_t *data;
} T_RawRes;

/* Query `dbtable` ("<db>/<table>") for `key` (hex, >= 32 bits, up to the table's
 * key_ln) and return every matching record. Returns NULL on invalid input or
 * allocation failure. `dbtable` and `key` stay owned by the caller (they are not
 * freed here); the caller owns the returned T_RawRes and must free both
 * result->data and result. */
T_RawRes *ldb_query_raw(char *dbtable, char *key);

/* Record handler matching ldb_record_handler_t (new_hash architecture): the
 * table is passed so a handler can derive subkey_ln = table->key_ln - LDB_KEY_LN.
 * Appends each record to the T_RawRes in `ptr`. */
bool ldb_dump_row(struct ldb_table *table, uint8_t *key, uint8_t *subkey, uint8_t *data, uint32_t size, int record_number, void *ptr);

#endif
