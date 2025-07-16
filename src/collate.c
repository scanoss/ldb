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
#include "ldb.h"
#include "mz.h"
#include "collate.h"
#include "logger.h"
#include "decode.h"
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

int ldb_collate_tuple_cmp(const void * a, const void * b)
{
	const tuple_t *va = *(tuple_t **) a;
	const tuple_t *vb = *(tuple_t **) b;
	
	/* Compare each byte until the end of the shorter record */
    for (int i = 0; i < 16; i++)
    {
		if (va->key[i] > vb->key[i]) 
			return 1;
        if (va->key[i] < vb->key[i]) 
			return -1;
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
		int error = ldb_node_write(out_table, new_sector, collate->last_key, collate->tmp_data, new_block_size, block_records);
		//abort in case of error
		if (error < 0)
			return false;
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

		uint32_t projected_size = buffer_ptr + rec_size  + collate->table_key_ln + (2 * LDB_PTR_LN) + out_table.ts_ln;

		/* Check if key is different than the last one */
		new_subkey = (memcmp(rec_key+LDB_KEY_LN, last_key+LDB_KEY_LN, subkey_ln) != 0);
		/* If node size is exceeded, initialize buffer */
		if (projected_size >= LDB_MAX_REC_LN)
		{
			/* Write buffer to disk and initialize buffer */
			if (rec_group_size > 0) uint16_write(buffer + rec_group_start + subkey_ln, rec_group_size);
			if (buffer_ptr) 
			{
				int error =ldb_node_write(out_table, new_sector, last_key, buffer, buffer_ptr, 0);
				//abort in case of error
				if (error < 0)
					return false;
			}
			buffer_ptr = 0;
			rec_group_start  = 0;
			rec_group_size   = 0;
			new_subkey = true;
		}

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
	if (buffer_ptr) 
	{
		int error= ldb_node_write(out_table, new_sector, last_key, buffer, buffer_ptr, 0);
		//abort in case of error
		if (error < 0)
			return false;
	}

	free(buffer);
	free(last_key);
	free(last_data);

	return true;
}

/**
 * @brief Import a list and write it into a file.
 * @param collate pointer to collate data structure.
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

		else if (collate->rec_width)
		{
			items = collate->data_ptr / collate->rec_width;
			size = collate->rec_width;
		}
		else
		{
			fprintf(stderr,"Warning collate rec_width undefined\n");
			return;
		}
		qsort(collate->data, items, size, ldb_collate_cmp);
}

static bool data_compare(char * a, char * b)
{
	char buffer_a[LDB_MAX_REC_LN] = "\0";
	char buffer_b[LDB_MAX_REC_LN] = "\0";
	
	if (!a || !b)
		return false;

	//printf("a: %s, b: %s\n",a ,b);
	while (*a && *b)
	{
		bool skip_field = false;
		while (*a && *a != ',')
		{	
			buffer_a[strlen(buffer_a)] = *a;
			a++;
		}

		if (strchr(buffer_a, '*') && strlen(buffer_a) < 4)
		{
			skip_field = true;
			log_debug("skipping");
		}
		
		while (*b && *b != ',')
		{
			buffer_b[strlen(buffer_b)] = *b;
			b++;
		}

		int r = strcmp(buffer_a, buffer_b);
		log_debug("<<<comparing: %s / %s : %d >>\n", buffer_a, buffer_b, r);
		if (!skip_field && r)
			return false;
		a++;
		b++;
		memset(buffer_a, 0, LDB_MAX_REC_LN);
		memset(buffer_b, 0, LDB_MAX_REC_LN);
	}

	return true;
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
bool key_in_delete_list(struct ldb_collate_data *collate, uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t * data, uint32_t size)
{
	/* Position pointer to start of second byte in the sorted del_key array */
	for (int i = 0; i < collate->del_tuples->tuples_number; i++)
	{ 		
		/*The keys are sorted, if I'm in another sector a must out*/
		if (collate->del_tuples->tuples[i]->key[0] > key[0])
			return false;
		else if (collate->del_tuples->tuples[i]->key[0] != key[0])
			continue;

		/* First byte is always the same, second too inside this loop. Compare bytes  2, 3 and 4 */
		int mainkey = memcmp(collate->del_tuples->tuples[i]->key + 1, key + 1, LDB_KEY_LN -1);
		if (mainkey != 0) 
			continue; //return false;
		
		/*For fixed records there is no subkey, so key hex will be empty*/
		char key_hex1[collate->in_table.key_ln * 2 + 1];
		char key_hex2[collate->in_table.key_ln * 2 + 1];
		ldb_bin_to_hex(subkey, subkey_ln, key_hex1);
		ldb_bin_to_hex(&collate->del_tuples->tuples[i]->key[LDB_KEY_LN], subkey_ln, key_hex2);
		
		/*Math the rest of the key*/
		if (!memcmp(subkey, &collate->del_tuples->tuples[i]->key[LDB_KEY_LN], subkey_ln))
		{
			bool result = true;
			if (collate->del_tuples->tuples[i]->data)
			{
				int char_to_skip = 0;
				char * data_key_hex = collate->del_tuples->tuples[i]->data;
				//compare secudary keys
				for (int i =0; i < collate->del_tuples->keys_number-1; i++)
				{
					data_key_hex += char_to_skip;
					char * key_end = strchr(data_key_hex, ',');
					if (!key_end)
						break;
					
					int len = key_end - data_key_hex;
					char_to_skip += len + 1;
					
					/*skip key from comparation*/
					if (len < 4 && strchr(data_key_hex,'*'))
					{
						continue;
					}
					uint8_t sec_key[collate->del_tuples->key_ln];
					ldb_hex_to_bin(data_key_hex, collate->del_tuples->key_ln * 2, sec_key);

					ldb_bin_to_hex(sec_key, collate->del_tuples->key_ln, key_hex1);
					ldb_bin_to_hex(data + collate->del_tuples->key_ln * i, collate->del_tuples->key_ln, key_hex2);
					result = !memcmp(data + collate->del_tuples->key_ln * i, sec_key, collate->del_tuples->key_ln);
					if (result)
						break;
				}

				if (result)
				{
					log_debug("Match found: %s / %s\n", key_hex1, key_hex2);
					if (collate->in_table.definitions & LDB_TABLE_DEFINITION_ENCRYPTED)
					{
						//if we are ignoring the data the record must be removed.
						if (strchr(collate->del_tuples->tuples[i]->data + char_to_skip, '*'))
							result = true;
						else
						{
							unsigned char tuple_bin[MAX_CSV_LINE_LEN];
							if(!decode && !ldb_decoder_lib_load())
								return false;

							int r_size = decode(DECODE_BASE64, NULL, NULL, collate->del_tuples->tuples[i]->data + char_to_skip, strlen(collate->del_tuples->tuples[i]->data) - char_to_skip, tuple_bin);
							if (r_size > 0) 
							{
								result = !memcmp(tuple_bin, data + (collate->del_tuples->keys_number - 1) * collate->del_tuples->key_ln, r_size);
								log_info("Var record %s was found.\n", collate->del_tuples->tuples[i]->data);
							}
							else
								result = false;
						}
					}
					else
					{
						if (collate->in_table.rec_ln == 0) //variable record, string comparation
						{ 
							char * aux = strndup((char*)data + (collate->del_tuples->keys_number - 1) * collate->del_tuples->key_ln, 
							size - (collate->del_tuples->keys_number - 1) * collate->del_tuples->key_ln);
							result = data_compare(collate->del_tuples->tuples[i]->data + char_to_skip, aux);
							free(aux);
						}
						else //fixed record, hex comparation
						{
							int data_ln = strlen(collate->del_tuples->tuples[i]->data);
							if (data_ln < LDB_KEY_LN * 2)
								continue;

							uint8_t * aux = calloc(1, collate->in_table.rec_ln);
							ldb_hex_to_bin(collate->del_tuples->tuples[i]->data, data_ln, aux);
							result = false; //we just want to remove a record inside data
							for(int ptr = 0; ptr < size; ptr += collate->in_table.rec_ln)
							{ 
								char test[33];
								ldb_bin_to_hex(data + ptr, 16, test);
								if (memcmp(aux, data + ptr, data_ln/2) == 0)
								{					
									log_info("Fixed record %s was found at %d\n", collate->del_tuples->tuples[i]->data, ptr);
									memset(data + ptr, 0, collate->in_table.rec_ln);
									collate->del_count++;
								}
							}
							free(aux);
						}
					}
				}
			}
			/* Increment del_count and return true */
			if (result)
			{
				collate->del_count++;
				return true;
			}
		}
	}

	return false;
}

/**
 * @brief LDB collate handler. Will be called for ldb_fetch_recordset in each iteration.
 * Execute the collate job, adding the new registers or deleting the keys from the delete list.
 * @param collate pointer to collate data structure 
 * @param key block key
 * @param subkey block subkey
 * @param subkey_ln block subkey lenght
 * @param data uint8_t pointer to data to be added
 * @param size data size
 * @return true
 */
bool ldb_collate_handler(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr)
{

	struct ldb_collate_data *collate = ptr;
	if (!collate->rec_width)
	{
		return true;
	}
	/* If main key has changed, collate and write list and reset data_ptr */
	if (collate->data_ptr && memcmp(key, collate->last_key, LDB_KEY_LN))
	{
		/* Sort records */
		ldb_collate_sort(collate);

		/* Import records */
		ldb_import_list(collate);

		/* Reset data pointer */
		collate->data_ptr = 0;
		collate->key_rec_count = 0;
	}
	else
	{
		collate->key_rec_count++;
	}

	/* If we exceed LDB_MAX_RECORDS, skip it */
	if (collate->key_rec_count > LDB_MAX_RECORDS)
	{
		if (collate->key_rec_count == LDB_MAX_RECORDS + 1)
		{
			char hex_val[(LDB_KEY_LN + subkey_ln) * 2 + 1];
			ldb_bin_to_hex(key, LDB_KEY_LN, hex_val);
			ldb_bin_to_hex(subkey, subkey_ln, hex_val + LDB_KEY_LN * 2);
			log_info("%s: Max list size exceeded\n",hex_val);
			collate->key_rec_count++;
		}
		return false;
	}

	/* Skip key if found among del_keys (DELETE command only) */
	if (collate->del_tuples)
	{
		if (key_in_delete_list(collate, key, subkey, subkey_ln, data, size)) return false;
	}

	/* Keep record */
	if (ldb_collate_add_record(collate, key, subkey, subkey_ln, data, size))
	{
		/* Show progress */
		time_t seconds = time(NULL);
		if ((seconds - collate->last_report) > COLLATE_REPORT_SEC)
		{
			log_debug("%s - %02x%02x%02x%02x: %'ld records read\n", collate->out_table.table, key[0], key[1], key[2], key[3], collate->rec_count);
			collate->last_report = seconds;
		}
	}
	else
	{
		log_debug("%s - %02x%02x%02x%02x: Ignored record with %d bytes\n", collate->out_table.table, key[0], key[1], key[2], key[3], LDB_KEY_LN + subkey_ln + size);
	}

	/* Save last key */
	memcpy(collate->last_key, key, LDB_KEY_LN);

	return false;
}

int ldb_collate_load_tuples_to_delete(job_delete_tuples_t * job, char * buffer, char * d, struct ldb_table table)
{
	char *delimiter = d;
	tuple_t **tuples = NULL;
	int tuples_index = 0;
    char * line = strtok(buffer, delimiter);
	int key_len = table.key_ln;

    while (line != NULL) 
	{
		if (line && strlen(line) >= key_len * 2)
		{
			tuples = realloc(tuples, ((tuples_index+1) * sizeof(tuple_t*)));
			tuples[tuples_index] = calloc(1, sizeof(tuple_t));
			ldb_hex_to_bin(line, key_len * 2, tuples[tuples_index]->key);

			if (strchr(line, ','))
			{
				char * data = strdup(line + key_len * 2 + 1);
				if (data && *data)
					tuples[tuples_index]->data = data;
			}
			tuples_index++;
		}
        line = strtok(NULL, delimiter);
    }
	job->tuples = tuples;
	job->tuples_number = tuples_index;
	job->keys_number = table.keys;
	job->key_ln = key_len;
	qsort(job->tuples, tuples_index, sizeof(tuple_t *), ldb_collate_tuple_cmp);
	
	log_info("Keys to delete %d:\n", tuples_index);
	
	for (int i = 0; i < job->tuples_number; i++)
	{
		char key_hex[MD5_LEN*2+1];
		ldb_bin_to_hex(job->tuples[i]->key, key_len, key_hex);
		log_info("<key: %s", key_hex);
		if (job->tuples[i]->data)
			log_info(" %s>\n", job->tuples[i]->data);
		else
			log_info(">\n");
	}

	return tuples_index;
}

bool ldb_collate_init(struct ldb_collate_data * collate, struct ldb_table table, struct ldb_table out_table, int max_rec_ln, bool merge, uint8_t sector)
{
	collate->data_ptr = 0;
	collate->table_key_ln = table.key_ln;
	collate->table_rec_ln = table.rec_ln;
	collate->max_rec_ln = max_rec_ln;
	collate->rec_count = 0;
	collate->key_rec_count = 0;
	collate->del_count = 0;
	collate->in_table = table;
	collate->out_table = out_table;
	memcpy(collate->last_key, "\0\0\0\0", 4);
	collate->last_report = 0;
	collate->merge = merge;
	collate->handler = NULL;
	collate->del_tuples = NULL;

	if (collate->table_rec_ln)
	{
		collate->rec_width = collate->table_rec_ln;
	}
	else
	{
		collate->rec_width = table.key_ln + max_rec_ln + 4;
	}

	/* Reserve space for collate data */
	collate->data = (char *)calloc(LDB_MAX_RECORDS * (collate->rec_width+10), 1);
	
	if (!collate->data)
		return false;

	collate->tmp_data = (char *)calloc(LDB_MAX_RECORDS * (collate->rec_width+10), 1);

	if (!collate->tmp_data)
		return false;

	/* Set global cmp width (for qsort) */
	ldb_cmp_width = max_rec_ln;

	/* Open (out) sector */
	collate->out_sector = ldb_open(out_table, &sector, "w+");
	if (!collate->out_sector)
		return false;
	
	return true;
}

void ldb_collate_sector(struct ldb_collate_data *collate, ldb_sector_t * sector)
{
	log_info("Collating %s/%s - sector %02x - %s\n", collate->in_table.db, collate->in_table.table, sector->id, sector->data == NULL ? "On disk" : "On RAM");
	/* Read each one of the (256 ^ 3) list pointers from the map */
	uint8_t k[LDB_KEY_LN];
	k[0] = sector->id;
	for (int k1 = 0; k1 < 256; k1++)
		for (int k2 = 0; k2 < 256; k2++)
			for (int k3 = 0; k3 < 256; k3++)
			{
				k[1] = k1;
				k[2] = k2;
				k[3] = k3;
				/* If there is a pointer, read the list */
				/* Process records */
				ldb_fetch_recordset_v2(sector, collate->in_table, k, true, ldb_collate_handler, collate);
			}

	/* Process last record/s */
	if (collate->data_ptr)
	{
		ldb_collate_sort(collate);
		ldb_import_list(collate);
	}

	/* Close .out sector */
	fclose(collate->out_sector);

	/* Move or erase sector */
	if (collate->merge)
		ldb_sector_erase(collate->in_table, k);
	else
		ldb_sector_update(collate->out_table, k);

	if (collate->del_count)
		log_info("%s - sector %02X: %'ld records deleted\n", collate->in_table.table, sector->id, collate->del_count);
	
	log_info("Table %s - sector %2x: collate completed with %'ld records\n", collate->in_table.table , sector->id, collate->rec_count);

	free(collate->data);
	free(sector->data);
	sector->data = NULL;
}

/**
 * @brief Execute the collate job
 * 
 * @param table LDB table to be processed
 * @param out_table Output LDB table
 * @param max_rec_ln Maximum record lenght
 * @param merge True for update a record, false to add a new one.
 * @param del_keys pointer to list of keys to be deleted.
 * @param del_ln number of keys to be deleted
 */
void ldb_collate(struct ldb_table table, struct ldb_table out_table, int max_rec_ln, bool merge, int p_sector, collate_handler handler)
{
	/* Start with sector 0, unless it is a delete command */
	uint8_t k0 = 0;
	if (p_sector >= 0)
		k0 = p_sector;

	long total_records = 0;
	setlocale(LC_NUMERIC, "");
	
	logger_dbname_set(table.db);
	/* Read each DB sector */
	do {
		log_info("Collating Table %s - Reading sector %02x\n", table.table, k0);
		struct ldb_collate_data collate;
		
		if (ldb_collate_init(&collate, table, out_table, max_rec_ln, merge, k0))
		{

			/* Load collate data structure */
			collate.handler = handler;
			collate.del_tuples = NULL;
			ldb_sector_t sector  = ldb_load_sector_v2(table, &k0);
			//skip unexistent sector.
			if (!sector.size)
				continue;

			ldb_collate_sector(&collate, &sector);
		}
		
		if (p_sector >=0)
			break;

		/* Exit here if it is a delete command, otherwise move to the next sector */
	} while (k0++ < 255);

	/* Show processed totals */
	if (p_sector >= 0)
		log_info("Table %s - sector %2x: collate completed with %'ld records\n", table.table , p_sector, total_records);
	else
		log_info("Table %s: collate completed with %'ld records\n", table.table, total_records);


	fflush(stdout);
}


/**
 * @brief Execute the collate job
 * 
 * @param table LDB table to be processed
 * @param out_table Output LDB table
 * @param max_rec_ln Maximum record lenght
 * @param merge True for update a record, false to add a new one.
 * @param del_keys pointer to list of keys to be deleted.
 * @param del_ln number of keys to be deleted
 */
void ldb_collate_delete(struct ldb_table table, struct ldb_table out_table, job_delete_tuples_t * delete, collate_handler handler)
{

	/* Start with sector 0, unless it is a delete command */
	uint8_t k0 = 0;
	
	/* Otherwise use the first byte of the first key */
	if (!delete)
		return;

	long total_records = 0;
	setlocale(LC_NUMERIC, "");
	
	logger_dbname_set(table.db);
	int k0_last = -1;
	/* Read each DB sector */
	for (int i = 0; i < delete->tuples_number; i++) 
	{
		k0 = *delete->tuples[i]->key;
		struct ldb_collate_data collate;
		if (k0 != k0_last && ldb_collate_init(&collate, table, out_table, 2048, false, k0))
		{
			log_info("Removing keys from Table %s - Reading sector %02x\n", table.table, k0);
			/* Load collate data structure */
			collate.handler = handler;
			collate.del_tuples = delete;
			collate.del_count = 0;
			ldb_sector_t sector = ldb_load_sector_v2(table, &k0);
			ldb_collate_sector(&collate, &sector);
			total_records += collate.del_count;
		}
		k0_last = k0;
		/* Exit here if it is a delete command, otherwise move to the next sector */
	} 

	/* Show processed totals */
	log_info("Table %s: cleanup completed with %'ld records\n", table.table, total_records);
	fflush(stdout);

}
