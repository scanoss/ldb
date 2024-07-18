// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/ldb_wrapper.h
 *
 * This file defines an entry point to query database from a shared library
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

#ifndef _LDB_WRAPPER_
#define _LDB_WRAPPER_

#include "./ldb/definitions.h"

#include "./ldb/definitions.h"
#include "./ldb/types.h"
#include "./ldb/mz.h"

typedef struct {
	uint32_t size;
    uint32_t capacity;
    uint8_t *data;
} T_RawRes;


T_RawRes * ldb_query_raw(char *dbtable, char *key);
bool ldb_dump_row(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr) ;

#endif