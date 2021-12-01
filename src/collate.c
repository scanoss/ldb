// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/collate.c
 *
 * Table COLLATE functions
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
  * @file collate.c
  * @date 19 Aug 2020 
  * @brief Implement de funtions used for collate ldb records
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/collate.c
  */

/**
 * @brief Compare two blocks. Compare each byte until the end of the shorter record.
 * 
 * @param a block a
 * @param b block b
 * @return 1 if a is bigger than b, -1 if b is bigger tha a, or 0 if they are equals.
 */
int ldb_collate_cmp(const void * a, const void * b)
{
	const uint8_t *va = a;
	const uint8_t *vb = b;

	/* Compare each byte until the end of the shorter record */
    for (int i = 0; i < ldb_cmp_width; i++)
    {
        if (va[i] > vb[i]) return 1;
        if (va[i] < vb[i]) return -1;
    }

    return 0;
}

/**
 * @brief Checks if two blocks of memory contain the same data, from last to first byte
 * 
 * @param a block a.
 * @param b block b.
 * @param bytes number of bytes to be compared
 * @return true if they are equals, false otherwise.
 */
bool ldb_reverse_memcmp(uint8_t *a, uint8_t *b, int bytes)
{
	for (int i = (bytes - 1); i >= 0; i--)
		if (*(a + i) != *(b + i)) return false;
	return true;
}

/**
 * @brief Eliminate duplicated records from data into tmp_data, returns new data size
 * 
 * @param collate pointer to buffer to be collated
 * @param ptr start position
 * @param size block number
 * @return int updated size after collate
 */
int ldb_eliminate_duplicates(struct ldb_collate_data *collate, long ptr, int size)
{
	int new_size = 0;
	int rec_ln = collate->table_rec_ln;
	bool skip = false;

	/* Recurse every record in collate->data + ptr */
	for (long i = 0; i < size; i += collate->table_rec_ln)
	{
		uint8_t *a = collate->data + ptr + i;
		uint8_t *b = collate->tmp_data + new_size;

		/* Check if duplicate */
		if (new_size) if (ldb_reverse_memcmp(a, b - rec_ln, rec_ln)) skip = true;

		/* Copy record */
		if (!skip)
		{
			memcpy(b, a, rec_ln);
			new_size += rec_ln;
		}
		skip = false;
	}
	return new_size;
}

/**
 * @brief import a list, collate and write to a file.
 * 
 * @param collate pointer to collate data strcture.
 * @return true succed
 */
bool ldb_import_list_fixed_records(struct ldb_collate_data *collate)
{
	FILE * new_sector = collate->out_sector;
	int max_per_node = (LDB_MAX_REC_LN - collate->rec_width) / collate->rec_width;

	struct ldb_table out_table = collate->out_table;
	long data_ptr = 0;

	/* Read data in "max_per_node" chunks */
	while (data_ptr < collate->data_ptr)
	{
		int block_size = collate->data_ptr - data_ptr;
		if (block_size > (max_per_node * collate->rec_width))
		{
			block_size = max_per_node * collate->rec_width;
		}

		/* Write node */
		int new_block_size = ldb_eliminate_duplicates(collate, data_ptr, block_size);
		uint16_t block_records = new_block_size / collate->table_rec_ln;
		ldb_node_write(out_table, new_sector, collate->last_key, collate->tmp_data, new_block_size, block_records);
		data_ptr += block_size;
	}

	return true;
}

/**
 * @brief import a list, collate and write to a file.
 * @param collate pointer to collate data strcture.
 * @return true succed
 */
bool ldb_import_list_variable_records(struct ldb_collate_data *collate)
{
	FILE * new_sector = collate->out_sector;
	uint8_t *buffer = malloc(LDB_MAX_NODE_LN);
	uint16_t buffer_ptr = 0;
	uint8_t *rec_key;
	uint8_t *last_key = calloc(collate->table_key_ln,1);
	uint16_t rec_group_start = 0;
	uint16_t rec_group_size = 0;
	int subkey_ln = collate->table_key_ln - LDB_KEY_LN;
	bool new_subkey = true;
	struct ldb_table out_table = collate->out_table;

	/* Last record checksum to skip duplicates */
	uint8_t *last_data = calloc(collate->rec_width, 1);
	uint16_t last_rec_size = 0;

	for (long data_ptr = 0; data_ptr < collate->data_ptr; data_ptr += collate->rec_width)
	{
		rec_key = collate->data + data_ptr;
		uint8_t *data = rec_key + out_table.key_ln + subkey_ln;
		uint16_t rec_size;

		if (collate->table_rec_ln) rec_size = collate->table_rec_ln;
		else rec_size = uint32_read(rec_key + collate->rec_width - LDB_KEY_LN);

		/* If record is duplicated, skip it */
		if (rec_size == last_rec_size) if (!memcmp(data, last_data, rec_size)) continue;

		/* Update last record */
		memcpy(last_data, data, rec_size);
		last_rec_size = rec_size;

		uint32_t projected_size = buffer_ptr + collate->rec_width + (2 * LDB_PTR_LN) + out_table.ts_ln;

		/* If node size is exceeded, initialize buffer */
		if (projected_size >= LDB_MAX_REC_LN)
		{
			/* Write buffer to disk and initialize buffer */
			if (rec_group_size > 0) uint16_write(buffer + rec_group_start + subkey_ln, rec_group_size);
			if (buffer_ptr) ldb_node_write(out_table, new_sector, last_key, buffer, buffer_ptr, 0);
			buffer_ptr = 0;
			rec_group_start  = 0;
			rec_group_size   = 0;
			new_subkey = true;
		}

		/* Check if key is different than the last one */
		if (!new_subkey) new_subkey = (memcmp(rec_key, last_key, collate->table_key_ln) != 0);

		/* New file id, start a new record group */
		if (new_subkey)
		{
			/* Write size of previous record group */
			if (rec_group_size > 0) uint16_write(buffer + rec_group_start + subkey_ln, rec_group_size);
			rec_group_start  = buffer_ptr;

			/* K: Add remaining part of key to buffer */
			memcpy(buffer + buffer_ptr, rec_key + LDB_KEY_LN, subkey_ln);
			buffer_ptr += subkey_ln;

			/* GS: Add record group size (zeroed) */
			uint16_t zero = 0;
			uint16_write(buffer + buffer_ptr, zero);
			buffer_ptr += 2;

			/* Update last_key */
			memcpy(last_key, rec_key, collate->table_key_ln);

			/* Update variables */
			rec_group_size   = 0;
		}

		/* Add record length to record */
		uint16_write(buffer + buffer_ptr, rec_size);
		buffer_ptr += 2;

		/* Add record to buffer */
		memcpy (buffer + buffer_ptr, data, rec_size);
		buffer_ptr += rec_size;
		rec_group_size += (2 + rec_size);
	}

	/* Write buffer to disk */
	if (rec_group_size > 0) uint16_write(buffer + rec_group_start + subkey_ln, rec_group_size);
	if (buffer_ptr) ldb_node_write(out_table, new_sector, last_key, buffer, buffer_ptr, 0);

	free(buffer);
	free(last_key);
	free(last_data);

	return true;
}

/**
 * @brief Import a list and write it in a file.
 * @param collate pointer to collate data strcture.
 * @return true succed
 */
bool ldb_import_list(struct ldb_collate_data *collate)
{
	if (collate->table_rec_ln) return ldb_import_list_fixed_records(collate);

	return ldb_import_list_variable_records(collate);
}

/**
 * @brief Add fixed records to a list
 * 
 * @param collate pointer to collate data structure 
 * @param key block key
 * @param subkey block subkey
 * @param subkey_ln block subkey lenght
 * @param data uint8_t pointer to data to be added
 * @param size data size
 * @return true
 */
bool ldb_collate_add_fixed_records(struct ldb_collate_data *collate, uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size)
{
	/* Size should be = N x rec_ln */
	if (size % collate->table_rec_ln) return false;

	int boundary = LDB_MAX_RECORDS * collate->rec_width;

	/* Iterate through recordset */
	for (int i = 0; i < size; i += collate->table_rec_ln)
	{
		/* Copy subkey */
		if (subkey_ln)
		{
			memcpy(collate->data + collate->data_ptr, subkey, subkey_ln);
			collate->data_ptr += subkey_ln;
		}

		/* Exit if boundary reached */
		if (collate->data_ptr + collate->table_rec_ln >= boundary) break;

		/* Copy record */
		memcpy(collate->data + collate->data_ptr, data + i, collate->table_rec_ln);
		collate->data_ptr += collate->table_rec_ln;

		collate->rec_count++;
	}
	return true;
}

/**
 * @brief Add variable records to a list
 * 
 * @param collate pointer to collate data structure 
 * @param key block key
 * @param subkey block subkey
 * @param subkey_ln block subkey lenght
 * @param data uint8_t pointer to data to be added
 * @param size data size
 * @return true
 */
bool ldb_collate_add_variable_record(struct ldb_collate_data *collate, uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size)
{
	/* Add record exceeds limit, skip it */
	if (size > collate->max_rec_ln) return false;

	/* Copy main key */
	memcpy(collate->data + collate->data_ptr, key, LDB_KEY_LN);
	collate->data_ptr += LDB_KEY_LN;

	/* Copy subkey */
	memcpy(collate->data + collate->data_ptr, subkey, subkey_ln);
	collate->data_ptr += subkey_ln;

	/* Copy record */
	memcpy(collate->data + collate->data_ptr, data, size);
	collate->data_ptr += collate->max_rec_ln;

	/* Copy record length */
	uint32_write(collate->data + collate->data_ptr, size);
	collate->data_ptr += 4;

	collate->rec_count++;
	return true;
}

/**
 * @brief Add record to a list
 * 
 * @param collate pointer to collate data structure 
 * @param key block key
 * @param subkey block subkey
 * @param subkey_ln block subkey lenght
 * @param data uint8_t pointer to data to be added
 * @param size data size
 * @return true
 */
bool ldb_collate_add_record(struct ldb_collate_data *collate, uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size)
{
	if (collate->table_rec_ln)
	{
		return ldb_collate_add_fixed_records(collate, key, subkey, subkey_ln, data, size);
	}
	return ldb_collate_add_variable_record(collate, key, subkey, subkey_ln, data, size);
}

/**
 * @brief Sort a list
 * 
 * @param collate point to collate data structure
 */
void ldb_collate_sort(struct ldb_collate_data *collate)
{
		if (collate->merge) return;

		/* Sort records */
		size_t items = 0;
		size_t size = 0;
		int subkey_ln = collate->table_key_ln - LDB_KEY_LN;

		if (collate->table_rec_ln)
		{
			items = collate->data_ptr / (collate->table_rec_ln + subkey_ln);
			size = collate->table_rec_ln + subkey_ln;
		}

		else
		{
			items = collate->data_ptr / collate->rec_width;
			size = collate->rec_width;
		}
		qsort(collate->data, items, size, ldb_collate_cmp);
}

/**
 * @brief Search for key+subkey in the del_keys blob. Search is lineal on a sorted array, with the aid of del_map
 * to speed up the search.
 * 
 * @param collate pointer to collate data structure 
 * @param key block key
 * @param subkey block subkey
 * @param subkey_ln block subkey lenght
 * @return true
 */
bool key_in_delete_list(struct ldb_collate_data *collate, uint8_t *key, uint8_t *subkey, int subkey_ln)
{
	/* Position pointer to start of second byte in the sorted del_key array */
	long key_ptr = collate->del_map[key[1]];
	int step = LDB_KEY_LN + subkey_ln;

	/* Travel through del_keys */
	while (key_ptr < collate->del_ln && collate->del_keys[key_ptr + 1] == key[1])
	{
		/* First byte is always the same, second too inside this loop. Compare bytes 3 and 4 */
		int mainkey = memcmp(collate->del_keys + key_ptr + 2, key + 2, 2);
		if (mainkey > 0) break;

		if (!mainkey) if (!memcmp(subkey, collate->del_keys + key_ptr + LDB_KEY_LN, subkey_ln))
		{
			/* Increment del_count and return true */
			collate->del_count++;
			return true;
		}

		key_ptr += step;
	}
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
bool ldb_collate_handler(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr)
{

	struct ldb_collate_data *collate = ptr;

	/* If main key has changed, collate and write list and reset data_ptr */
	if (collate->data_ptr) if (memcmp(key, collate->last_key, LDB_KEY_LN))
	{
		/* Sort records */
		ldb_collate_sort(collate);

		/* Import records */
		ldb_import_list(collate);

		/* Reset data pointer */
		collate->data_ptr = 0;
	}

	/* If we exceed LDB_MAX_RECORDS, skip it */
	if ((collate->data_ptr + collate->rec_width) > (LDB_MAX_RECORDS * collate->rec_width))
	{
		printf("%02x%02x%02x%02x: Max list size exceeded\n", key[0], key[1], key[2], key[3]);
		return false;
	}

	/* Skip key if found among del_keys (DELETE command only) */
	if (collate->del_ln)
	{
		if (key_in_delete_list(collate, key, subkey, subkey_ln)) return false;
	}

	/* Keep record */
	if (ldb_collate_add_record(collate, key, subkey, subkey_ln, data, size))
	{
		/* Show progress */
		time_t seconds = time(NULL);
		if ((seconds - collate->last_report) > COLLATE_REPORT_SEC)
		{
			printf("%02x%02x%02x%02x: %'ld records read\n", key[0], key[1], key[2], key[3], collate->rec_count);
			collate->last_report = seconds;
		}
	}
	else
	{
		printf("%02x%02x%02x%02x: Ignored record with %d bytes\n", key[0], key[1], key[2], key[3], LDB_KEY_LN + subkey_ln + size);
	}

	/* Save last key */
	memcpy(collate->last_key, key, LDB_KEY_LN);

	return false;
}


/**
 * @brief Distribute list of keys to be deleted into 256 arrays matching the second byte from the key.
 *	(the first byte is the same in all keys)
 * 
 * @param del_keys //TODO
 * @param del_ln //TODO
 * @param subkey_ln //TODO
 * @return long* //TODO
 */
long *load_del_map(uint8_t *del_keys, long del_ln, int subkey_ln)
{
	int step = LDB_KEY_LN + subkey_ln;
	long *map = calloc(sizeof(long), 256);
	uint8_t last_k = 0;

	/* Travel through del_keys and fill up map with pointers based on second byte of the key */
	long key_ptr = 0;
	while (key_ptr < del_ln)
	{
		if (del_keys[key_ptr + 1] != last_k)
		{
			last_k = del_keys[key_ptr + 1];
			map[last_k] = key_ptr;
		}
		key_ptr += step;
	}
	return map;
}

/**
 * @brief // TODO
 * 
 * @param table // TODO
 * @param out_table // TODO
 * @param max_rec_ln // TODO
 * @param merge // TODO
 * @param del_keys // TODO
 * @param del_ln // TODO
 */
void ldb_collate(struct ldb_table table, struct ldb_table out_table, int max_rec_ln, bool merge, uint8_t *del_keys, long del_ln)
{

	long *del_map = NULL;

	/* Start with sector 0, unless it is a delete command */
	uint8_t k0 = 0;

	/* Otherwise use the first byte of the first key */
	if (del_ln) k0 = *del_keys;

	long total_records = 0;
	setlocale(LC_NUMERIC, "");

	/* Read each DB sector */
	do {
		printf("Reading sector %02x\n", k0);
		uint8_t *sector = ldb_load_sector(table, &k0);
		if (sector)
		{

			/* Load collate data structure */
			struct ldb_collate_data collate;

			collate.data_ptr = 0;
			collate.table_key_ln = table.key_ln;
			collate.table_rec_ln = table.rec_ln;
			collate.max_rec_ln = max_rec_ln;
			collate.rec_count = 0;
			collate.del_count = 0;
			collate.out_table = out_table;
			memcpy(collate.last_key, "\0\0\0\0", 4);
			collate.last_report = 0;
			collate.merge = merge;

			/* Load delete keys map to speed up key lookup */
			if (del_ln) del_map = load_del_map(del_keys, del_ln, table.key_ln - LDB_KEY_LN);
			collate.del_keys = del_keys;
			collate.del_ln = del_ln;
			collate.del_map = del_map;

			if (collate.table_rec_ln)
			{
				collate.rec_width = collate.table_rec_ln;
			}
			else
			{
				collate.rec_width = table.key_ln + max_rec_ln + 4;
			}

			/* Reserve space for collate data */
			collate.data = (char *) calloc(LDB_MAX_RECORDS * collate.rec_width, 1);
			collate.tmp_data = (char *) calloc(LDB_MAX_RECORDS * collate.rec_width, 1);

			/* Set global cmp width (for qsort) */
			ldb_cmp_width = max_rec_ln;

			/* Open (out) sector */
			collate.out_sector = ldb_open(out_table, &k0, "r+");

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
						/* If there is a pointer, read the list */
						if (ldb_map_pointer_pos(k))
						{
							/* Process records */
							ldb_fetch_recordset(sector, table, k, true, ldb_collate_handler, &collate);
						}
					}

			/* Process last record/s */
			if (collate.data_ptr)
			{
				ldb_collate_sort(&collate);
				ldb_import_list(&collate);
			}

			total_records += collate.rec_count;
			printf("%'ld records read\n", collate.rec_count);

			/* Close .out sector */
			fclose(collate.out_sector);

			/* Move or erase sector */
			if (collate.merge) ldb_sector_erase(table, k);
			else ldb_sector_update(out_table, k);

			if (collate.del_count) printf("%'ld records deleted\n", collate.del_count);

			free(collate.data);
			free(collate.tmp_data);
			free(sector);
		}

		/* Exit here if it is a delete command, otherwise move to the next sector */
	} while (k0++ < 255 && !del_ln);

	/* Show processed totals */
	printf("Collate completed with %'ld records\n", total_records);

	fflush(stdout);

	if (del_map) free(del_map);
}
