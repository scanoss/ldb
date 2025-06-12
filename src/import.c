// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/import.c
 *
 * Data importation into LDB Knowledge Base
 *
 * Copyright (C) 2018-2021 SCANOSS.COM
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
 * @file import.c
 * @date 25 October 2021
 * @brief Implement the functions used for import
 */
#include <stdio.h>
#include <sys/time.h>
#include <sys/file.h> 
#include <libgen.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "import.h"
#include "./ldb.h"
#include "bsort.h"
#include "join.h"
#include <pthread.h>
#include <sys/sysinfo.h>
#include "ignored.h"
#include <signal.h>
#include "collate.h"
#include "logger.h"
#include "ldb_error.h"
#include "decode.h"
#define REC_SIZE_LEN 2

pthread_mutex_t lock;

int max_threads = 0;
pthread_t * threads_list;
static long IGNORED_WFP_LN = sizeof(IGNORED_WFP);
double progress_timer = 0;

/**
 * @brief Sort a csv file invokinkg a new process and executing a sort command
 *
 * @param file_path file path to be processed
 * @param sort true to skip sort
 * @return true if succed
 */
bool csv_sort(ldb_importation_config_t * config)
{
	if (!config->opt.params.sort || !ldb_file_size(config->csv_path)  )
		return false;

	/* Assemble command */
	char *command = malloc(5 * LDB_MAX_PATH);
	snprintf(command, 5 * LDB_MAX_PATH,"sort -T %s -u -o %s.tmp %s && mv %s.tmp %s", config->opt.params.tmp_path, config->csv_path, config->csv_path,
																					config->csv_path, config->csv_path);
	
	log_info("Sorting %s\n", config->csv_path);
	
	FILE *p = popen(command, "r");
	if (p)
		pclose(p);
	else
	{
		printf("Cannot execute %s\n", command);
		free(command);
		return false;
	}
	free(command);
	return true;
}

/**
 * @brief Execute bsort over a file
 *
 * @param file_path pointer to file path
 * @param sort
 * @return true
 */
bool bin_sort(char *file_path, bool sort)
{
	if (!ldb_file_size(file_path))
		return false;
	if (!sort)
		return true;
	log_info("Sorting %s\n", file_path);
	return bsort(file_path);
}


/**
 * @brief Checks if two blocks of memory contain the same data, from last to first byte
 *
 * @param a
 * @param b
 * @param bytes
 * @return true
 */
bool reverse_memcmp(uint8_t *a, uint8_t *b, int bytes)
{
	for (int i = (bytes - 1); i >= 0; i--)
		if (*(a + i) != *(b + i))
			return false;
	return true;
}

bool check_file_extension(char * path, bool bin_mode)
{
	if (bin_mode || (decode && access(path, F_OK) != 0))
		strcat(path, ".enc");
	
	return true;
}

/**
 * @brief Extract the numeric value of the two nibbles making the file name
 *
 * @param filename
 * @return uint8_t
 */
uint8_t first_byte(char *filename)
{
	long byte = strtol(basename(filename), NULL, 16);

	/* Check that 0 means 00 and not an invalid name */
	if (!byte)
		if (memcmp(basename(filename), "00", 2))
			byte = -1;

	/* Failure to retrieve the first wfp byte is fatal */
	if (byte < 0 || byte > 255)
	{
		printf("Invalid file name %s\n", filename);
		exit(EXIT_FAILURE);
	}

	return (uint8_t)byte;
}

/**
 * @brief Show progress in stdout
 *
 * @param prompt string message to show
 * @param count actual count
 * @param max maximum value
 * @param percent true for percent mode
 */
void progress(char * path, char * table, size_t count, size_t max, bool percent)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	double tmp = (double)(t.tv_usec) / 1000000 + (double)(t.tv_sec);
	logger_basic(NULL);
	if ((tmp - progress_timer) < 5)
		return;
	pthread_mutex_lock(&lock);
	progress_timer = tmp;
	pthread_mutex_unlock(&lock);

	if (percent)
	{
		logger_basic("%s", table);
		log_info("Importing %s to table %s: %.2f%%", path, table, ((double)count / (double)max) * 100);
	}
	else
	{
		logger_basic("%s\n", table);
		log_info("Importing %s to table %s: %lu", path, table, count);
	}
	//fflush(stdout);
}

/**
 * @brief Import a raw wfp file which simply contains a series of 21-byte records containing wfp(3)+md5(16)+line(2). While the wfp is 4 bytes,
 * the first byte is the file name
 *
 * @param db_name DB name
 * @param filename filename string
 * @param skip_delete true to avoid delete
 * @return true is succed
 */
int ldb_import_snippets(ldb_importation_config_t * config)
{
	/* Table definition */
	struct ldb_table oss_wfp;
	strcpy(oss_wfp.db, config->dbname);
	strcpy(oss_wfp.table, config->table);
	oss_wfp.key_ln = 4;
	oss_wfp.rec_ln = 18;
	oss_wfp.ts_ln = 2;
	oss_wfp.tmp = config->opt.params.overwrite;

	/* Progress counters */
	uint64_t totalbytes = ldb_file_size(config->csv_path);
	int reccounter = 0;
	size_t bytecounter = 0;
	int tick = 10000; // activate progress every "tick" records

	/* raw record length = wfp crc32(3) + file md5(16) + line(2) = 21 bytes */
	int raw_ln = 21;

	/* First three bytes are bytes 2nd-4th of the wfp) */
	int rec_ln = raw_ln - 3;

	/* First byte of the wfp is the file name */
	uint8_t key1 = first_byte(config->csv_path);

	/* File should contain 21 * N bytes */
	if (ldb_file_size(config->csv_path) % 21)
	{
		printf("File %s does not contain 21-byte records\n", config->csv_path);
		exit(EXIT_FAILURE);
	}

	/* Load ignored wfps into boolean array */
	bool *bl = calloc(256 * 256 * 256, 1);
	for (int i = 0; i < IGNORED_WFP_LN; i += 4)
		if (IGNORED_WFP[i] == key1)
			bl[IGNORED_WFP[i + 1] + IGNORED_WFP[i + 2] * 256 + IGNORED_WFP[i + 3] * 256 * 256] = true;

	FILE *in, *out;
	out = NULL;

	in = fopen(config->csv_path, "rb");
	if (in == NULL)
		return -1;

	/* Lock DB */
	char lock_file[LDB_MAX_PATH];
	sprintf(lock_file, "%s.%s",oss_wfp.db,oss_wfp.table);
	//ldb_lock(lock_file);

	uint64_t wfp_counter = 0;
	uint64_t ignore_counter = 0;

	/* We keep the last read key to group wfp records */
	uint8_t last_wfp[4] = "\0\0\0\0";
	uint8_t tmp_wfp[4] = "\0\0\0\0";
	*last_wfp = key1;
	*tmp_wfp = key1;

	/* This will store the LDB record, which cannot be larger than 65535 md5s(16)+line(2) (>1Mb) */
	uint8_t *record = malloc(256 * 256 * rec_ln);
	uint32_t record_ln = 0;

	/* The first byte of the wfp crc32(4) is the actual file name containing the records
		 We'll read 50 million wfp at a time (making a buffer of about 1Gb) */
	uint32_t buffer_ln = 50000000 * raw_ln;
	uint8_t *buffer = malloc(buffer_ln);

	/* Create table if it doesn't exist */
	if (!ldb_table_exists(config->dbname, config->table))
		ldb_create_table_new(config->dbname, config->table, 4, rec_ln, 1, LDB_TABLE_DEFINITION_STANDARD);

	/* Open ldb */
	out = ldb_open(oss_wfp, last_wfp, "r+");

	bool first_read = true;
	uint32_t bytes_read = 0;

	while ((bytes_read = fread(buffer, 1, buffer_ln, in)) > 0)
	{
		for (int i = 0; (i + raw_ln) <= bytes_read; i += raw_ln)
		{
			uint8_t *wfp = buffer + i;
			uint8_t *rec = buffer + i + 3;

			if (bl[wfp[0] + wfp[1] * 256 + wfp[2] * 256 * 256])
			{
				ignore_counter++;
				continue;
			}

			bool new_key = (!reverse_memcmp(last_wfp + 1, wfp, 3));
			bool full_node = ((record_ln / rec_ln) >= 65535);

			/* Do we have a new key, or is the buffer full? */
			if (new_key || full_node || first_read)
			{
				first_read = false;

				/* If there is a buffer, write it */
				if (record_ln)
				{
					int error = ldb_node_write(oss_wfp, out, last_wfp, record, record_ln, (uint16_t)(record_ln / rec_ln));
					//abort in case of error
					if (error < 0)
						return error;
				}
				wfp_counter++;

				/* Initialize record */
				memcpy(record, rec, rec_ln);
				record_ln = rec_ln;

				/* Save key */
				memcpy(last_wfp + 1, wfp, 3);
			}

			/* Add file id to existing record */
			else
			{
				/* Skip duplicated records. Since md5 records to be imported are sorted, it will be faster
					 to compare them from last to first byte. Also, we only compare the 16 byte md5 */
				if (record_ln > 0)
					if (!reverse_memcmp(record + record_ln - rec_ln, rec, 16))
					{
						memcpy(record + record_ln, rec, rec_ln);
						record_ln += rec_ln;
						wfp_counter++;
					}
			}

			/* Update progress every "tick" records */
			if (++reccounter > tick)
			{
				bytecounter += (rec_ln * reccounter);
				progress(config->csv_path,config->table, bytecounter, totalbytes, true);
				reccounter = 0;
			}
		}
	}
	log_info("%s: %'lu wfp imported, %'lu ignored\n", config->csv_path, wfp_counter, ignore_counter);

	if (record_ln)
	{
		int error = ldb_node_write(oss_wfp, out, last_wfp, record, record_ln, (uint16_t)(record_ln / rec_ln));
		//abort in case of error
		if (error < 0)
			return error;
	}

	if (out)
		ldb_close_unlock(out);

	fclose(in);
	if (config->opt.params.delete_after_import)
		unlink(config->csv_path);

	free(record);
	free(buffer);
	free(bl);

	if (config->opt.params.overwrite)
		ldb_sector_update(oss_wfp, last_wfp);

	/* Lock DB */
	//ldb_unlock(lock_file);

	return 0;
}

/**
 * @brief Count number of comma delimited fields in data
 *
 * @param data pointer to data
 * @return int count
 */
int csv_fields(char *data)
{
	int commas = 0;
	while (*data)
		if (*data++ == ',')
			commas++;
	return commas + 1;
}

/**
 * @brief Returns a pointer to field n in data
 *
 * @param n filed number
 * @param data data buffer
 * @return char* to field
 */
char *field_n(int n, char *data)
{
	int commas = 0;
	while (*data)
		if (*data++ == ',')
			if (++commas == n - 1)
				return data;
	return NULL;
}

/**
 * @brief Extract binary item ID (and optional first field binary ID) from CSV line
 * where the first field is the hex itemid and the second could also be hex (if is_file_table)
 *
 * @param line pointer to line
 * @param first_byte fisrt byte to be added
 * @param got_1st_byte true if the line got the 1st byte.
 * @param itemid pointer to item id
 * @param field2 pointer to second field
 * @param is_file_table true for processing a file table
 * @return true if succed
 */
bool file_id_to_bin(char *line, uint8_t first_byte, bool got_1st_byte, uint8_t *itemid, uint8_t *field2, bool is_file_table)
{

	/* File ID is only the last 15 bytes (first byte should be in the file name) */
	if (line[30] == ',')
	{
		if (!got_1st_byte)
		{
			printf("Key is incomplete. File name does not contain the first byte\n");
			return false;
		}

		/* Concatenate first_byte and 15 remaining bytres into itemid */
		else
		{
			/* Add 1st byte */
			*itemid = first_byte;

			/* Convert remaining 15 bytes */
			ldb_hex_to_bin(line, MD5_LEN_HEX - 2, itemid + 1);

			/* Convert urlid if needed (file table) */
			if (is_file_table)
				ldb_hex_to_bin(line + (MD5_LEN_HEX - 2 + 1), MD5_LEN_HEX, field2);
		}
	}

	/* Entire file ID included */
	else
	{
		/* Convert item id */
		ldb_hex_to_bin(line, MD5_LEN_HEX, itemid);

		/* Convert url id if needed (file table) */
		if (is_file_table)
			ldb_hex_to_bin(field_n(2, line), MD5_LEN_HEX, field2);
	}

	uint8_t zero_md5[MD5_LEN] = {0xd4,0x1d,0x8c,0xd9,0x8f,0x00,0xb2,0x04,0xe9,0x80,0x09,0x98,0xec,0xf8,0x42,0x7e}; //empty string md5

	if (!memcmp(itemid,zero_md5, MD5_LEN)) //the md5 key of an empty string must be skipped.
		return false;
	
	memset(zero_md5, 0, MD5_LEN); // al zeros md5 must be skippets

	if (!memcmp(itemid,zero_md5, MD5_LEN))
		return false;


	return true;
}

/**
 * @brief Vaerify if a string is a valid hexadecimal number
 *
 * @param str input string
 * @param bytes bytes to be processed
 * @return true if it is valid
 */
bool valid_hex(char *str, int bytes)
{
	for (int i = 0; i < bytes; i++)
	{
		char h = str[i];
		if (h < '0' || (h > '9' && h < 'a') || h > 'f')
			return false;
	}
	return true;
}


int ldb_import_mz(ldb_importation_config_t * job)
{
	char dest_path[LDB_MAX_PATH];
	sprintf(dest_path, "%s/%s/%s/%s", ldb_root, job->dbname, job->table, basename(job->csv_path));
	
	if (!ldb_table_exists(job->dbname, job->table))
		ldb_create_table_new(job->dbname, job->table, 16, 0, job->opt.params.keys_number, LDB_TABLE_DEFINITION_MZ |
						(job->opt.params.binary_mode ? LDB_TABLE_DEFINITION_ENCRYPTED : LDB_TABLE_DEFINITION_STANDARD));
	else
	{
			/* Create table structure for bulk import (32-bit key) */
		char db_table[LDB_MAX_NAME];
		snprintf(db_table,LDB_MAX_NAME-1, "%s/%s", job->dbname, job->table);
		struct ldb_table oss_bulk = ldb_read_cfg(db_table);
		if (oss_bulk.keys < 1 || oss_bulk.definitions == LDB_TABLE_DEFINITION_UNDEFINED)
		{
			oss_bulk.keys = job->opt.params.keys_number;
			ldb_write_cfg(oss_bulk.db, oss_bulk.table, oss_bulk.key_ln, oss_bulk.rec_ln, oss_bulk.keys, 
						job->opt.params.binary_mode ? LDB_TABLE_DEFINITION_MZ | LDB_TABLE_DEFINITION_ENCRYPTED :  LDB_TABLE_DEFINITION_MZ | LDB_TABLE_DEFINITION_STANDARD);
			log_info("Table %s config file was updated\n", oss_bulk.table);
		}
	}
	
	return (ldb_bin_join(job->csv_path, dest_path, job->opt.params.overwrite, false, job->opt.params.delete_after_import));
}

/**
 * @brief Import a CSV file into the LDB database
 *
 * @param job pointer to minr job
 * @param filename file name string
 * @param table table name string
 * @param nfields number of fileds
 * @return true if succed
 */
int ldb_import_csv(ldb_importation_config_t * job)
{
	bool bin_mode = false;
	bool is_compressed = false;
	bool skip_csv_check = !job->opt.params.validate_fields;
	bool sectors_modified[256] = {false};
	if (job->opt.params.binary_mode || strstr(job->csv_path, ".enc"))
	{
		bin_mode = true;
		skip_csv_check = true;
		if (strstr(job->csv_path, ".cmp"))
			is_compressed = true;
	}
	
	FILE *fp = fopen(job->csv_path, "r");
	if (fp == NULL)
	{
		log_info("File does not exist %s\n", job->csv_path);
		return -1;
	}

	/* A CSV line should contain at least an MD5, a comma separator per field and a LF */
	int min_line_size = 2 * MD5_LEN + (skip_csv_check > 0 ? 0 : job->opt.params.csv_fields);

	/* Node size is a 16-bit int */
	int node_limit = 65536;

	uint8_t *itemid = calloc(MD5_LEN, 1);
	uint8_t *field2 = calloc(MD5_LEN, 1);
	uint8_t *item_buf = malloc(LDB_MAX_NODE_LN);
	uint8_t *item_lastid = calloc(MD5_LEN * 2 + 1, 1);
	uint16_t item_ptr = 0;
	FILE *item_sector = NULL;
	uint16_t item_rg_start = 0; // record group size
	uint16_t item_rg_size = 0;	// record group size
	char last_id[MD5_LEN * 2 + 1 -2]; //save last 30th chars from the last md5.
	memset(last_id, 0, sizeof(last_id));

	/* Counters */
	uint32_t imported = 0;
	uint32_t skipped = 0;

	uint64_t totalbytes = ldb_file_size(job->csv_path);
	size_t bytecounter = 0;

	/* Get 1st byte of the item ID from csv filename (if available) */
	uint8_t first_byte = 0;
	bool got_1st_byte = false;
	if (valid_hex(basename(job->csv_path), 2))
		got_1st_byte = true;
	ldb_hex_to_bin(basename(job->csv_path), 2, &first_byte);

	/* Create table if it doesn't exist */
	pthread_mutex_lock(&lock);
	if (!ldb_database_exists(job->dbname))
		ldb_create_database(job->dbname);

	int table_definitions = LDB_TABLE_DEFINITION_STANDARD;

	if (bin_mode)
		table_definitions |= LDB_TABLE_DEFINITION_ENCRYPTED;
	if (is_compressed)
		table_definitions |= LDB_TABLE_DEFINITION_COMPRESSED;
	if (!ldb_table_exists(job->dbname, job->table))
		ldb_create_table_new(job->dbname, job->table, 16, 0, job->opt.params.keys_number, table_definitions);
	pthread_mutex_unlock(&lock);

	/* Create table structure for bulk import (32-bit key) */
	char db_table[LDB_MAX_NAME];
	snprintf(db_table,LDB_MAX_NAME-1, "%s/%s", job->dbname, job->table);

	struct ldb_table oss_bulk = ldb_read_cfg(db_table);
	if (oss_bulk.keys < 1 || oss_bulk.definitions < 0)
	{
		oss_bulk.keys = job->opt.params.keys_number;
		ldb_write_cfg(oss_bulk.db, oss_bulk.table, oss_bulk.key_ln, oss_bulk.rec_ln, oss_bulk.keys, table_definitions);
		log_info("Table %s config file was updated\n", oss_bulk.table);
	}
	else if (table_definitions != oss_bulk.definitions)
	{
		log_info("The existent table definitions do not match with the table being imported, the file %s will be skipped.\nVerify %s.cfg file and try again\n",
		job->csv_path, job->table);
		return LDB_ERROR_CSV_WRONG_ENCODING;
	}
/* NOTE: the ldb table MUST BE written with key_ln = 4, and read with key_ln=16. ODO: IMPROVE IT, is this a bug?*/
	oss_bulk.key_ln = 4; 

	if (job->opt.params.overwrite)
		oss_bulk.tmp = true;

	int field2_ln = (oss_bulk.keys - 1) * MD5_LEN;
	char last_url_id[MD5_LEN_HEX+1] = "\0";

	/* Lock DB */
	char lock_file[LDB_MAX_PATH];
	sprintf(lock_file, "%s.%s",oss_bulk.db,oss_bulk.table);
	//ldb_lock(lock_file);
	
	/*sort the csv*/
	csv_sort(job);
	int line_number = 0;
	bool first_record = true;
	char *line = NULL;
	size_t len = 0;
	ssize_t lineln;
	while ((lineln = getline(&line, &len, fp)) != -1)
	{
		bytecounter += lineln;
		line_number++;

		if (lineln < job->opt.params.csv_fields)
		{
			log_debug("%s: Line %d -- Skipped, the line %s is too short (size %d)\n", job->csv_path, line_number, line, lineln);
			skipped++;
			continue;			
		}
		//skip lines starting with non alphanumeric char
		if (!isalnum(line[0]))
		{
			log_debug("%s: Line %d -- Skipped, Non alphanumeric char %d on line %s\n", job->csv_path, line_number, line[0], line);
			skipped++;
			continue;			
		}
		//skip keys with the incorrect lenght.
		char * first_comma = strchr(line, ',');
		if (!first_comma)
		{
			log_debug("%s: Line %d -- Skipped, wrong csv format on line %s .\n", job->csv_path, line_number, line);
			skipped++;
			continue;
		}
		int key_len = first_comma - line;
		if (key_len != MD5_LEN_HEX && key_len != MD5_LEN_HEX - 2)
		{
			log_debug("%s: Line %d -- Skipped, %d Incorrect key lenght.\n", job->csv_path, line_number, key_len);
			skipped++;
			continue;
		}
		/* Skip records with sizes out of range */
		if (lineln > MAX_CSV_LINE_LEN || lineln < min_line_size)
		{
			log_debug("%s: Line %d -- Skipped, %ld exceed MAX line size %d.\n", job->csv_path, line_number, lineln, MAX_CSV_LINE_LEN);
			skipped++;
			continue;
		}

		/* Trim trailing chr(10) */
		if (line[lineln - 1] == 10)
			lineln--;
		line[lineln] = 0;

		/* Check if this ID is the same as last */
		bool dup_id = false;
		if (!memcmp(last_id, line, MD5_LEN * 2 - 2)) //compare 30 chars of the md5
			dup_id = true;
		else
			memcpy(last_id, line, MD5_LEN * 2 - 2); //copy 30 chars of the md5

		/* First CSV field is the data key. Data starts with the second CSV field */
		char *data = field_n(2, line);
		bool skip = false;

		if (!data)
		{
			log_debug("%s: Line %d -- Skipped, data is missed %d.\n", job->csv_path, line_number);
			skipped++;
			continue;
		}

		/* File table will have the url id as the second field, which will be
			 converted to binary. Data then starts on the third field. Also file extensions
			 are checked and ruled out if ignored */
		if (oss_bulk.keys > 1)
		{
			/* Skip line if the URL is the same as last, importing unique files per url */
			if (dup_id && *last_url_id && !memcmp(data, last_url_id, MD5_LEN * 2))
			{
				log_debug("%s: Line %d -- Skipped, repeated URL ID.\n", job->csv_path, line_number);
				skip = true;
			}
			else
				memcpy(last_url_id, data, MD5_LEN * 2);
			
			if (job->opt.params.csv_fields > 2)
			{
				data = field_n(3, line);
				if (!data)
				{
					log_debug("%s: Error in line: %d, data is missing -- %s Skipped\n", job->csv_path, line_number, line);
					skipped++;
				}
			}
			else
				data = NULL;
		}

		/* Calculate record size */
		int r_size = 0;
		unsigned char data_bin[MAX_CSV_LINE_LEN];
		if (data)
		{
			if (bin_mode)
			{	
				if (decode)
				{
					r_size = decode(DECODE_BASE64,NULL, NULL, data, strlen(data), data_bin);
					if (r_size <= 0)
					{
						log_debug("Error: failed to decode line %s. Skipping\n", line);
						skip = true;
					}
				}
				else
					ldb_error("libscanoss_encoder.so it is not available, \".enc\" files cannot be processed\n");
			}
			else
			{
				/* Calculate record size */
				r_size = strlen(data);
			}
			/* Check if number of fields matches the expectation */
			if (!skip_csv_check && (csv_fields(line) != job->opt.params.csv_fields))
			{
				log_debug("%s: Line %d -- Skipped, Missing CSV fields. Expected: %d.\n",job->csv_path, line_number, job->opt.params.csv_fields);
				skip = true;
			}

			if (skip)
			{
				skipped++;
				continue;
			}
		}

		if (data || (oss_bulk.keys > 1 && job->opt.params.csv_fields < 3))
		{
			/* Convert id to binary (and 2nd field too if needed (files table)) */
			if (!file_id_to_bin(line, first_byte, got_1st_byte, itemid, field2, job->opt.params.keys_number > 1 ? true : false))
			{
				log_debug("%s: failed to parse key, line number: %d\n", job->csv_path, line_number);
				skipped++;
				continue;
			}

			/* Check if we have a whole new key (first 4 bytes), or just a new subkey (last 12 bytes) */
			bool new_key = first_record ? true : (memcmp(itemid, item_lastid, 4) != 0);
			first_record = false;
			bool new_subkey = new_key ? true : (memcmp(itemid, item_lastid, MD5_LEN) != 0);
			/* If we have a new main key, or we exceed node size, we must flush and and initialize buffer */
			if (new_key || (item_ptr + 5 * LDB_PTR_LN + MD5_LEN + 2 * REC_SIZE_LEN + r_size) >= node_limit)
			{
				/* Write buffer to disk and initialize buffer */
				if (item_rg_size > 0)
					uint16_write(item_buf + item_rg_start + 12, item_rg_size);
				
				if (item_ptr)
				{
					if (!item_sector)
					{
						item_sector = ldb_open(oss_bulk, item_lastid, "r+");
						sectors_modified[item_lastid[0]] = true;
					}
					else
					{
						int error = ldb_node_write(oss_bulk, item_sector, item_lastid, item_buf, item_ptr, 0);
						//abort in case of error
						if (error < 0)
						{
							log_info("failed to process file %s, last line %s\n", job->csv_path, line);
							return error;
						}
					}
				}
				/* Open new sector if needed */
				if (*itemid != *item_lastid || (*itemid == 0 && !item_ptr))
				{
					if (item_sector)
						ldb_close_unlock(item_sector);
					item_sector = ldb_open(oss_bulk, itemid, "r+");
					sectors_modified[itemid[0]] = true;
				}

				item_ptr = 0;
				item_rg_start = 0;
				item_rg_size = 0;
				new_subkey = true;
			}

						/* New file id, start a new record group */
			if (new_subkey)
			{
				/* Write size of previous record group */
				if (item_rg_size > 0)
					uint16_write(item_buf + item_rg_start + 12, item_rg_size);
				item_rg_start = item_ptr;

				/* K: Add remaining part of key to buffer */
				memcpy(item_buf + item_ptr, itemid + 4, MD5_LEN - LDB_KEY_LN);
				item_ptr += MD5_LEN - LDB_KEY_LN;

				/* GS: Add record group size (zeroed) */
				uint16_t zero = 0;
				uint16_write(item_buf + item_ptr, zero);
				item_ptr += REC_SIZE_LEN;

				/* Update item_lastid */
				memcpy(item_lastid, itemid, MD5_LEN);

				/* Update variables */
				item_rg_size = 0;
			}

			/* Add record length to record */
			uint16_write(item_buf + item_ptr, r_size + field2_ln);
			item_ptr += REC_SIZE_LEN;

			/* Add url id to record */
			memcpy(item_buf + item_ptr, field2, field2_ln);
			item_ptr += field2_ln;
			item_rg_size += (field2_ln + REC_SIZE_LEN);
			if (data)
			{
				/* Add record to buffer */
				if (!bin_mode)
					memcpy(item_buf + item_ptr, data, r_size);
				else
					memcpy(item_buf + item_ptr, data_bin, r_size);
				item_ptr += r_size;
				item_rg_size += r_size;
			}
			imported++;
		}
		progress(job->csv_path, job->table, bytecounter, totalbytes, true);
	}

	/* Flush buffer */
	if (item_rg_size > 0)
		uint16_write(item_buf + item_rg_start + MD5_LEN - LDB_KEY_LN, item_rg_size);
	
	if (item_ptr)
	{
		if (!item_sector)
		{
			item_sector = ldb_open(oss_bulk, itemid, "r+");
			sectors_modified[itemid[0]] = true;
		}
		
		int error = ldb_node_write(oss_bulk, item_sector, item_lastid, item_buf, item_ptr, 0);
		//abort in case of error
		if (error < 0)
			return error;
	}
	
	if (item_sector)
		ldb_close_unlock(item_sector);

	log_info("%s: %u records imported, %u skipped\n", job->csv_path, imported, skipped);

	if (fclose(fp))
		fprintf(stderr,"error closing %d\n", fileno(fp));
	
	if (job->opt.params.delete_after_import)
		unlink(job->csv_path);

	if (line)
		free(line);

	free(itemid);
	free(item_buf);
	free(item_lastid);
	free(field2);

	
	if (job->opt.params.overwrite)
	{
		for (int i=0; i < 256; i++)
		{
			if (sectors_modified[i])
			{
				log_info("Overwriting sector %02x of %s\n", i, job->table);
				uint8_t k = i;
				ldb_sector_update(oss_bulk, &k);
			}
		}

	}
	/* Lock DB */
//	ldb_unlock(lock_file);
	return LDB_ERROR_NOERROR;
}

/**
 * @brief Wipes table before importing (-O)
 *
 * @param table path <to table>
 * @param job pointer to mner job
 */
void wipe_table(ldb_importation_config_t *config)
{
	if (!config->opt.params.overwrite)
		return;

	bool is_mz = config->opt.params.is_mz_table;

	char path[2 * LDB_MAX_PATH] = "\0";
	sprintf(path, "%s/%s/%s", ldb_root, config->dbname, config->table);

	printf("Wiping  %s\n", path);

	if (!ldb_table_exists(config->dbname, config->table))
	{
		fprintf(stderr, "Table %s cannot be wiped, path does not exist\n", config->table);
		return;
	}

	for (int i = 0; i < (is_mz ? 65535 : 256); i++)
	{
		if (is_mz)
		{
			sprintf(path, "%s/%s/%s/%04x.mz", ldb_root, config->dbname, config->table, i);
			check_file_extension(path, config->opt.params.binary_mode);
		}
		else
			sprintf(path, "%s/%s/%s/%02x.ldb", ldb_root, config->dbname, config->table, i);
		unlink(path);
	}
}

static char * version_get_daily(char * json)
{
	if (!json)
		return NULL;

	char * daily = strstr(json, "\"daily\":");

	if (daily)
	{
		daily = strchr(daily,':');
		daily = (char*) memrchr(daily,'"', 3);
		
		if(!daily)
			return NULL;

		char * daily_date = strndup(daily+1, 8);
		if (strstr(daily_date, "N/A"))
		{
			free(daily_date);
			return NULL;
		}
		return daily_date;
	}

	return NULL;

}

static char * version_get_monthly(char * json)
{
	if (!json)
		return NULL;

	char * monthly = strstr(json, "\"monthly\":");

	if (monthly)
	{
		monthly = strchr(monthly,':');
		monthly = memrchr(monthly,'"', 4);
		if (!monthly)
			return NULL;
		char * monthly_date = strndup(monthly+1, 5);
		if (strstr(monthly_date, "N/A"))
		{
			free(monthly_date);
			return NULL;
		}
		return monthly_date;
	}

	return NULL;
}

static char * version_file_open(char * path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	char * file_content = calloc(len + 1, 1);
	fread(file_content, len, 1, f);
	fclose(f);

	return file_content;
}


/**
 * @brief Import version.json
 * @param job pointer to minr job
 * @return true if succed
 */
bool version_import(ldb_importation_config_t *job)
{
	#define JSON_CONTENT  "{\"monthly\":\"%s\", \"daily\":\"%s\"}"
	char path[LDB_MAX_PATH] = "\0";
	snprintf(path, LDB_MAX_PATH, "%s/version.json",  job->path);

	if (!ldb_file_exists(path))
	{
		char * in_path = strdup(job->path);
		snprintf(path, LDB_MAX_PATH, "%s/version.json", dirname(in_path));
		free(in_path);
		if(!ldb_file_exists(path))
		{
			logger_basic("Cannot find version file in path %s\n",job->path);
			return false;
		}
	}

	char * vf_import = version_file_open(path);

	char * daily_date_i = version_get_daily(vf_import);
	char * monthly_date_i = version_get_monthly(vf_import);

	int test_len = strlen(vf_import);
	
	if (daily_date_i)
		test_len -= (strlen(daily_date_i) + strlen("\"daily\":"));
	if (monthly_date_i)
		test_len -= (strlen(monthly_date_i) + strlen("\"monthly\":"));
	//exit if cannot find daily or monthly or there are an excess of characteres in the json
	if ((!daily_date_i && !monthly_date_i) || test_len > 10)
	{
		logger_basic("Failed to process version file: %s\n", vf_import);
		free(vf_import);
		free(daily_date_i);
		free(monthly_date_i);
		return false;
	}
	sprintf(path, "%s/%s/version.json", ldb_root, job->dbname);
	char * vf_actual = version_file_open(path);		
	char * daily_date_o = version_get_daily(vf_actual);
	char * monthly_date_o = version_get_monthly(vf_actual);


	if (!daily_date_i && daily_date_o)
	{
		daily_date_i = daily_date_o;
		daily_date_o = NULL;
	}
	
	if (!monthly_date_i && monthly_date_o)
	{
		monthly_date_i = monthly_date_o;
		monthly_date_o = NULL;
	}


	FILE *f = fopen(path, "w+");
	
	if (!f)
	{
		logger_basic("Cannot create destination file: %s\n", path);
		return false;
	}

	fprintf(f, JSON_CONTENT, monthly_date_i == NULL ? "N/A" : monthly_date_i, daily_date_i == NULL ? "N/A" :  daily_date_i);
	fclose(f);
	free(daily_date_i);
	free(daily_date_o);
	free(monthly_date_i);
	free(monthly_date_o);
	free(vf_actual);
	free(vf_import);

	return true;
}

const char * config_parameters[] = {
									"FILE_DEL",
									"KEYS",
									"OVERWRITE",
									"SORT",
									"VALIDATE_VERSION",
									"VERBOSE",
									"MZ",
									"BIN",
									"WFP",
									"FIELDS",
									"VALIDATE_FIELDS",
									"THREADS",
									"COLLATE",
									"MAX_RECORD",
									"MAX_RAM_PERCENT",
									"TMP_PATH",
									};

#define CONFIG_PARAMETERS_NUMBER  (sizeof(config_parameters) / sizeof(config_parameters[0]))
#define LDB_IMPORTATION_CONFIG_DEFAULT {.delete_after_import = 0,\
										.keys_number = 1,\
										.overwrite = 0,\
										.sort = 1,\
										.version_validation = 1,\
										.verbose = 0,\
										.is_mz_table = 0,\
										.binary_mode = 0,\
										.is_wfp_table = 0,\
										.csv_fields = 1,\
										.validate_fields = 1,\
										.threads = get_nprocs() / 2,\
										.collate = 0,\
										.collate_max_rec = 1024,\
										.collate_max_ram_percent = 50}

#define LDB_IMPORTATION_CONFIG_UNDEFINED {.delete_after_import = -1,\
										.keys_number = -1,\
										.overwrite = -1,\
										.sort = -1,\
										.version_validation = -1,\
										.verbose = -1,\
										.is_mz_table = -1,\
										.binary_mode = -1,\
										.is_wfp_table = -1,\
										.csv_fields = -1,\
										.validate_fields = -1,\
										.threads = -1,\
										.collate = -1,\
										.collate_max_rec = -1,\
										.collate_max_ram_percent = 50}

bool ldb_importation_config_parse(import_params_t * opt, char * line)
{
	if (!opt)
		return false;
//first normalize the line
	char normalized[LDB_MAX_COMMAND_SIZE];
	char no_spaces[LDB_MAX_COMMAND_SIZE];
	char * line_c = line;
	int i = 0;
	do
	{
		//skip spaces
		if (*line_c == ' ')
			continue;
		// remove casing
		no_spaces[i] = *line_c;
		normalized[i] = toupper(*line_c);
		i++;
	} while (*(line_c++) && i < LDB_MAX_COMMAND_SIZE);
	
	normalized[i] = 0;

	for (int i = 0; i < CONFIG_PARAMETERS_NUMBER; i++)
	{
		char * param = strstr(normalized, config_parameters[i]);
		int val = 0;
		if (!param || !(*(param-1) == ',' || *(param-1) == '(')) //the previous char must be a comma or a parentesis
			continue;
		param = strchr(param,'=');
		
		if (!strcmp(config_parameters[i], "TMP_PATH"))
		{
			strncpy(opt->params.tmp_path,no_spaces + (param - normalized) + 1, LDB_MAX_PATH);
			//remove spurius ")" or "," from path TODO: improve
			char * c = strchr(opt->params.tmp_path,',');
			if (c)
				*c = 0;
			else
			{
				 c = strrchr(opt->params.tmp_path,')');	
				 if (c)
				 	*c = 0;
			} 
		}
		else if (sscanf(param,"=%d", &val))
		{
			opt->params_arr[i] = val;
		}

		//printf("%d - found %s = %d\n", i, config_parameters[i], val);
	}
	return true;
}

//cmd_in take precedency of the global cfg
static void opt_add(const import_params_t * cmd_in, import_params_t * cfg)
{
	import_params_t def = {.params = LDB_IMPORTATION_CONFIG_DEFAULT};
	for (int i = 0; i < IMPORT_PARAMS_NUMBER; i++)
	{
		if (i == IMPORT_PARAMS_NUMBER -1 && strcmp(cmd_in->params.tmp_path, def.params.tmp_path))
		{
			strcpy(cfg->params.tmp_path, cmd_in->params.tmp_path);
		}
		else if (cmd_in->params_arr[i] > -1)
		{
			cfg->params_arr[i] = cmd_in->params_arr[i];
		}
	}
}

bool ldb_create_db_config_default(char * dbname)
{
	#define DEFAULT_CONFIG_STRING "GLOBAL: (VALIDATE_FIELDS=1, VALIDATE_VERSION=1, SORT=1, FILE_DEL=0, OVERWRITE=0, WFP=0, MZ=0, VERBOSE=0, THREADS=%d, COLLATE=0, MAX_RECORD=2048, MAX_RAM_PERCENT=50, TMP_PATH=/tmp)\n"\
					"sources: (MZ=1, KEYS=1)\n"\
					"notices: (MZ=1, KEYS=1)\n"\
					"attribution: (KEYS=1, FIELDS=2)\n"\
					"purl: (KEYS=1, OVERWRITE=1, VALIDATE_FIELDS=0)\n"\
					"dependency: (KEYS=1, FIELDS=5, OVERWRITE=1)\n"\
					"license: (KEYS=1, FIELDS=3)\n"\
					"copyright: (KEYS=1, FIELDS=3)\n"\
					"vulnerability: (KEYS=1, FIELDS=10, OVERWRITE=1)\n"\
					"quality: (KEYS=1, FIELDS=3)\n"\
					"cryptography: (KEYS=1, FIELDS=3)\n"\
					"url: (KEYS=1, FIELDS=8)\n"\
					"file: (KEYS=2, FIELDS=3)\n"\
					"pivot: (KEYS=2, FIELDS=2, VALIDATE_FIELDS=0)\n"\
					"semgrep: (KEYS=1, FIELDS=5)\n"\
					"wfp: (WFP=1, KEYS=1)\n"
	
	char config_path[LDB_MAX_PATH] = "";
	
	ldb_prepare_dir(LDB_CFG_PATH);
	
	sprintf(config_path,"%s%s.conf", LDB_CFG_PATH, dbname);
	FILE *cfg = fopen(config_path, "w+");
	char * config = NULL;
	int threads = get_nprocs() / 2;
	asprintf(&config, DEFAULT_CONFIG_STRING, threads > 0 ? threads : 1);
	if (cfg)
	{
		fputs(config, cfg);
		free(config);
		fclose(cfg);
		return true;
	}
	free(config);
	return false;
}

void log_table_config(char * name, import_params_t * opt)
{
	if (!(name && opt) || !*name)
		return;
	log_info("%s configuration: (KEYS=%d, VALIDATE_FIELDS=%d, FIELDS=%d, VALIDATE_VERSION=%d, SORT=%d, FILE_DEL=%d, OVERWRITE=%d, WFP=%d, MZ=%d, VERBOSE=%d, THREADS=%d, COLLATE=%d, MAX_RECORD=%d, MAX_RAM_PERCENT=%d, TMP_PATH=%s)\n",
	name, opt->params.keys_number, opt->params.validate_fields, opt->params.csv_fields, opt->params.version_validation, opt->params.sort, opt->params.delete_after_import, opt->params.overwrite, opt->params.is_wfp_table, opt->params.is_mz_table, opt->params.verbose,
	opt->params.threads, opt->params.collate, opt->params.collate_max_rec, opt->params.collate_max_ram_percent, opt->params.tmp_path);
}

static int load_import_config(ldb_importation_config_t * config, import_params_t * common)
{
	char config_path[LDB_MAX_PATH] = "";
	sprintf(config_path,"%s%s.conf", LDB_CFG_PATH, config->dbname);
	int result = -1;
	bool found = false;
	import_params_t def = {.params = LDB_IMPORTATION_CONFIG_DEFAULT};
	config->opt = def;
	common->params = def.params;
	
	if (ldb_file_exists(config_path))
	{
		FILE *cfg = fopen(config_path, "r");
		if (cfg)
		{
			char * line = NULL;
    		size_t len = 0;
    		ssize_t read;
			while ((read = getline(&line, &len, cfg)) != -1) 
			{
				char * separator = strchr(line, ':');
			
				if (!separator)
					continue;

				char table_test[LDB_MAX_NAME] = "\0";
				strncpy(table_test, line, separator - line);

				const char global_def[]= "GLOBAL";			
				if (!strcmp(table_test, global_def) && result < 0) // force global config on the top of the file
				{
				//	if (config->opt.params.verbose)
				//		fprintf(stderr, "The table %s is defined at %s as GLOBAL, some parameter may be overwritten\n", config->table, config_path);
					
					ldb_importation_config_parse(common, line + strlen(global_def));
					opt_add(common, &config->opt);
				}
				else
				{
					result++;
				}

			if (!strcmp(table_test, config->table))
				{
				//	if (config->opt.params.verbose)
				//		fprintf(stderr, "The table %s is defined at %s, some parameter may be overwritten\n", config->table, config_path);
					//check if the KEYS parameter is defined
					if (strstr(line, "KEYS"))
					{
						ldb_importation_config_parse(&config->opt, line + strlen(config->table));
					}
					else
					{
						fprintf(stderr, "Error in table %s definitions from %s\n", table_test, config_path);
						ldb_error("\"KEYS\" field must be defined. Please solve the error and retry the importation\n");
					}
					found = true;
					free(line);	
					break;
				}

    		}
		}
		fclose(cfg);

	}
	if (found)
		return result;
	else
		return -1;
}

static bool check_system_available_ram(struct ldb_table kb, uint8_t sector, int collate_max_ram_percent)
{
    FILE* file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        perror("Error opening /proc/meminfo");
        return false;
    }

    char line[256];
	unsigned long totalMemory = 0;
    unsigned long availableMemory = 0;
	unsigned long freeMemory = 0;
    
	while (fgets(line, sizeof(line), file)) {

		if (sscanf(line, "MemTotal: %lu", &totalMemory) == 1)
            continue;
		if (sscanf(line, "MemFree: %lu", &freeMemory) == 1) 
            continue;
        if (sscanf(line, "MemAvailable: %lu", &availableMemory) == 1) 
            break;
    }

	if (collate_max_ram_percent <= 0)
		collate_max_ram_percent = 50;

	int max_collate_ram = (totalMemory * (100 - collate_max_ram_percent)) / 100;

    fclose(file);
	
	char * path = ldb_sector_path(kb, &sector, "r");

	if (!path)
	{
		log_info("Failed to load sector %02x\n", sector);
		return false;
	}
	uint64_t sector_size = ldb_file_size(path) / 1024;
	
	free(path);
	log_info("max collate ram : %d", max_collate_ram);

    log_debug("Collate sector: %02x - sector size: %lu - Free memory: %lu - Available Memory: %lu \n", sector, sector_size  / 1024, freeMemory / 1024, availableMemory / 1024);
	if ((availableMemory < (sector_size * 2)  && freeMemory < sector_size * 1.5))
	{
		log_info("Not enough memory to allocate the sector %02x. Requested %ld - Free RAM %ld\n", sector, sector_size * 2, freeMemory);
		return false;
	}

	if (availableMemory < max_collate_ram)
	{
		log_info("Not enough memory to allocate the sector %02x. Available RAM (%ld) is lower than max collate ram (%ld)\n", sector, availableMemory, max_collate_ram);
		return false;
	}

	return true;
}

int import_collate_sector(ldb_importation_config_t *config)
{

	if (!config->opt.params.collate || config->opt.params.collate_max_rec < LDB_KEY_LN)
		return LDB_ERROR_NOERROR;

	char dbtable[LDB_MAX_NAME];
	snprintf(dbtable, LDB_MAX_NAME, "%s/%s", config->dbname, config->table);

	if (ldb_valid_table(dbtable))
	{
		/* Assembly ldb table structure */
		struct ldb_table ldbtable = ldb_read_cfg(dbtable);
		struct ldb_table tmptable = ldb_read_cfg(dbtable);
		tmptable.tmp = true;
		tmptable.key_ln = LDB_KEY_LN;

		int max_rec_len = config->opt.params.is_wfp_table == 1 ? 18 : config->opt.params.collate_max_rec;

		if (ldbtable.rec_ln && ldbtable.rec_ln != max_rec_len)
		{
			log_info("E076 Max record length should equal fixed record length (%d)\n", ldbtable.rec_ln);
			return LDB_ERROR_RECORD_LENGHT_INVAID;
		}
		else if (max_rec_len < ldbtable.key_ln)
		{
			log_info("E076 Max record length cannot be smaller than table key\n");
			return LDB_ERROR_RECORD_LENGHT_INVAID;
		}

		char *ptr = NULL;
		char *filename = basename(config->csv_path);
		//extract the sector from the file name. If there is no sector (example license.csv) process the compleate table.
		long sector_number = strtol(filename, &ptr, 16);
		if (ptr - filename < 2 || (ptr && *ptr != '.'))
			sector_number = -1;

		logger_basic("Collating - %s", dbtable);

		if (sector_number < 0)
		{
			log_info("Collating table %s - all sectors, Max record size: %d\n", dbtable, sector_number, max_rec_len);
			ldb_collate(ldbtable, tmptable, max_rec_len, false, sector_number, NULL);
			return 0;
		}
		
		log_info("Collating table %s - sector %02x, Max record size: %d\n", dbtable, sector_number, max_rec_len);
		if ((ldbtable.definitions > 0 && ldbtable.definitions & LDB_TABLE_DEFINITION_MZ) ||
			 (ldbtable.definitions == LDB_TABLE_DEFINITION_UNDEFINED && config->opt.params.is_mz_table))
		{
			ldb_mz_collate(ldbtable, sector_number);
		}
		else if (sector_number >= 0)
		{
			pthread_mutex_lock(&lock);
			struct ldb_collate_data collate;
			uint8_t k0 = sector_number;
			//uint8_t *sector_mem = NULL;
			ldb_sector_t sector = {.data=NULL, .size = 0, .id = sector_number};

			bool init_ok = ldb_collate_init(&collate, ldbtable, tmptable, max_rec_len, false, k0);
			if (!init_ok)
				log_info("Collate init failed for sector %d\n", k0);

			if (init_ok && check_system_available_ram(ldbtable, k0, config->opt.params.collate_max_ram_percent))
				//sector_mem = ldb_load_sector(ldbtable, &k0);
				sector = ldb_load_sector_v2(ldbtable, &k0);

			pthread_mutex_unlock(&lock);
			if (init_ok)
				ldb_collate_sector(&collate, &sector);
			else
			{
				log_info("ERROR: failed to allocate memory to collate sector %02x\n", k0);
				return LDB_ERROR_MEM_NOMEM;
			}
		}
	}
	return LDB_ERROR_NOERROR;
}

#define DEFAULT_TMP_PATH "/tmp/"
int ldb_import(ldb_importation_config_t * job)
{
	int result = -1;
	ldb_importation_config_t config = *job;

	if (strstr(config.csv_path, ".mz"))
		config.opt.params.is_mz_table = true;
	else
		config.opt.params.is_mz_table = false;
	
	if (strstr(config.csv_path, ".enc"))
		config.opt.params.binary_mode = true;
	else
		config.opt.params.binary_mode = false;

	if (strstr(config.csv_path, ".bin"))
		config.opt.params.is_wfp_table = true;
	else
		config.opt.params.is_wfp_table = false;

	if (config.opt.params.binary_mode)
	{
		pthread_mutex_lock(&lock);
		if(!decode)
			ldb_decoder_lib_load();
		pthread_mutex_unlock(&lock);
	}

	if (!config.opt.params.tmp_path)
	{
		sprintf(config.opt.params.tmp_path, DEFAULT_TMP_PATH);
	}
	
	if (config.opt.params.is_mz_table)
	{
		result = ldb_import_mz(&config);
	}
	else if (config.opt.params.is_wfp_table)
	{
		if (bin_sort(config.csv_path, config.opt.params.sort))
			result = ldb_import_snippets(&config);
	}
	else
	{
		result = ldb_import_csv(&config);
	}
	
	if (result == LDB_ERROR_NOERROR)
		result = import_collate_sector(&config);
	
	return result;
}

static int sector_from_path(char * path)
{
	char *ptr = NULL;
	char * filename = basename(path);
	long sector = strtol(filename, &ptr, 16);
	if (ptr - filename < 2 || (ptr && *ptr != '.'))
			sector = -1;
	
	return sector;
}

static char * table_name_from_path(char * path)
{
	char * table_name = strrchr(path, '/');
	char * out = NULL;
	if (table_name)
		table_name++;
	else
		table_name = path;
			
	char * dot = strchr(table_name, '.');
	if (dot)
		out = strndup(table_name, dot - table_name);
	else
		out = strdup(table_name);
	
	return out;
}

#define LDB_DEFAULT_TABLES_NUMBER 20
struct ldb_importation_jobs_s
{
	ldb_importation_config_t ** job;
	char dbname[LDB_MAX_NAME];
	int number;
	int sorted[LDB_DEFAULT_TABLES_NUMBER];
	int unsorted[LDB_DEFAULT_TABLES_NUMBER];
	int unsorted_index;
	import_params_t * user_opt;
	import_params_t * global_opt;
};

static void recurse_directory(struct ldb_importation_jobs_s * jobs, char *name, char * father)
{
	DIR *dir;
	struct dirent *entry;
	bool read = false;
//Skip "extra" folder if it is present.
	if (father && strstr(father, "extra"))
		return;
		
	if (!(dir = opendir(name))) 
		return;

	while ((entry = readdir(dir)))
	{
		if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;

		read = true;
		char path[LDB_MAX_PATH] = "\0";
		sprintf (path, "%s/%s", name, entry->d_name);
			
		if (entry->d_type == DT_DIR)
		{
			father = entry->d_name;
			recurse_directory(jobs, path, father);
		}
		else if (ldb_file_exists(path))
		{
			if (strstr(path, ".json"))
				continue;

			int sector = sector_from_path(path);
			char * table_name = sector >= 0 ? strdup(father) : table_name_from_path(path);
			//printf("Path %s - Name %s - %d Prev %s\n", path, table_name, jobs->number, jobs->number > 0 ? jobs->job[jobs->number - 1]->table : "empty");
			if (jobs->number == 0 || (strcmp(jobs->job[jobs->number - 1]->table, table_name)))
			{
				/*create new job*/
				jobs->job = realloc(jobs->job, ((jobs->number+1) * sizeof(ldb_importation_config_t*)));
				jobs->job[jobs->number] = calloc(1, sizeof(ldb_importation_config_t));
				/* load default config*/
				import_params_t opt_def = {.params = LDB_IMPORTATION_CONFIG_DEFAULT};
				jobs->job[jobs->number]->opt = opt_def;
				strcpy(jobs->job[jobs->number]->dbname, jobs->dbname);
				strncpy(jobs->job[jobs->number]->table, table_name, LDB_MAX_NAME);
				
				/*if the job is only one csv file, the file path is filled*/
				if (sector < 0)
					strncpy(jobs->job[jobs->number]->csv_path, path, LDB_MAX_PATH);
				else
					memset(jobs->job[jobs->number]->csv_path, 0, LDB_MAX_PATH);
				
				/*dir of the job*/
				snprintf(jobs->job[jobs->number]->path, LDB_MAX_PATH, "%s", dirname(path));
				
				//load of the configuration of the table (if it is defined).
				int sort = load_import_config(jobs->job[jobs->number], jobs->global_opt);
				//force stdin command priority
				opt_add(jobs->user_opt, &jobs->job[jobs->number]->opt);
				
				if (sort >= 0)
					jobs->sorted[sort] = jobs->number;
				else
				{
					jobs->unsorted[jobs->unsorted_index] = jobs->number;
					jobs->unsorted_index++;
				}
				
				jobs->number++;
			}
			free(table_name);
		}
	}

	if (read) 
	{
		closedir(dir);
	}
}


volatile int num_threads = 0;

void * thread_process_sector(void * arg)
{
	ldb_importation_config_t * job = arg;
	ldb_import(job);
	free(job);
	pthread_exit(NULL);
}
void threads_end(pthread_t * tlist)
{
	
	for (int j =0; j < max_threads; j++)
	{
		if (!tlist[j])
			continue;
		if (pthread_join(tlist[j], NULL) == 0)
		{
			pthread_mutex_lock(&lock);
			num_threads--;
			pthread_mutex_unlock(&lock);
		}
	
		tlist[j] = 0;
	}
}

int thread_request_or_wait(pthread_t * tlist)
{
	if (num_threads < max_threads)
		return num_threads;

	while(num_threads >= max_threads)
	{
		for (int j =0; j < max_threads; j++)
		{
			if (!tlist[j])
				continue;
			if (pthread_tryjoin_np(tlist[j], NULL) == 0)
			{
				pthread_mutex_lock(&lock);
				num_threads--;
				pthread_mutex_unlock(&lock);
				tlist[j] = 0;
				return j;
			}
		}
	}
	return -1;
}

volatile bool aborting = false;
// Signal handler function
void signalHandler(int signal) {
    if (aborting)
		exit(EXIT_FAILURE);
	aborting = true;

	if (signal == SIGINT) 
	{
        system("clear");
		printf("\n\n Safe abort, waiting threads to finish\n");
        threads_end(threads_list);
		exit(EXIT_FAILURE);
    }
}

pthread_t thread_start(ldb_importation_config_t * job, pthread_t * tlist)
{
	pthread_t tid;

	int t = thread_request_or_wait(tlist);
	pthread_mutex_lock(&lock);
	ldb_importation_config_t * job_cpy = malloc(sizeof(ldb_importation_config_t));
	memcpy(job_cpy,job,sizeof(ldb_importation_config_t));

	int result = pthread_create(&tid, NULL, thread_process_sector, (void*) job_cpy);
	pthread_mutex_unlock(&lock);

	if (result != 0)
	{
		printf("<<<<pthread failed %d>>>\n", result);
		free(job_cpy);
		return 0;
	}
	else
	{
		tlist[t] = tid;
		num_threads++;
	}

	return tid;
}

bool process_sectors(ldb_importation_config_t * job, pthread_t * tlist) {
    DIR *dir;
    struct dirent *ent;

	/*Process one file with multiples sectors inside*/
	if (*job->csv_path)
	{
		if (ldb_file_exists(job->csv_path))
		{
			pthread_t pid = thread_start(job, tlist);
			if (pid == 0)
				return ldb_import(job);
			else
				return true;
		}
		else
		{
			log_info("Could not be able to find the file: %s\n", job->csv_path);
			return false;
		}
	}
	
	/*Process a directory with one sector per file*/
    if ((dir = opendir(job->path)) != NULL) 
	{
		while ((ent = readdir(dir)) != NULL) 
		{
            if (!strcmp(ent->d_name,".") || !strcmp(ent->d_name,"..")) 
				continue;

			if (ent->d_type == DT_REG) 
			{
				snprintf(job->csv_path, LDB_MAX_PATH, "%s/%s", job->path, ent->d_name);
				pthread_t tid = thread_start(job, tlist);
				if (tid == 0)
					ldb_import(job);
            } 
        }

        closedir(dir);
    } 
	else 
	{
       	log_info("Cannot open directory: %s\n", job->path);
		return false;
    }
	return true;
}

void print_jobs(struct ldb_importation_jobs_s * jobs)
{
	log_info("\n Tables to be processed:\n");
	for (int i=0; i < LDB_DEFAULT_TABLES_NUMBER; i++)
	{
		if (jobs->sorted[i] != -1)
		{
			log_info("	%s: %s\n", jobs->job[jobs->sorted[i]]->table, 
									*jobs->job[jobs->sorted[i]]->csv_path != 0 ? jobs->job[jobs->sorted[i]]->csv_path : jobs->job[jobs->sorted[i]]->path);
		}
	}
	
	for (int i=0; i < LDB_DEFAULT_TABLES_NUMBER; i++)
	{
		if (jobs->unsorted[i] != -1)
		{
			log_info("	%s: %s\n", jobs->job[jobs->unsorted[i]]->table, 
									*jobs->job[jobs->unsorted[i]]->csv_path != 0 ? jobs->job[jobs->unsorted[i]]->csv_path : jobs->job[jobs->unsorted[i]]->path);
		}
	}

}

bool ldb_import_command(char * dbtable, char * path, char * config)
{
	if (!ldb_file_exists(path) && !ldb_dir_exists(path))
	{
		fprintf(stderr, "Error: file or directory %s not exist\n", path);
		return false;
	}

	signal(SIGINT, signalHandler);
	srand(time(NULL));
	import_params_t user_opt = {.params = LDB_IMPORTATION_CONFIG_UNDEFINED};
	import_params_t global_opt = {.params = LDB_IMPORTATION_CONFIG_UNDEFINED};
	ldb_importation_config_t job = {.opt.params = LDB_IMPORTATION_CONFIG_DEFAULT, .csv_path = "\0", .path = "\0"};

	char *table = strrchr(dbtable, '/');
	if (table)
		strncpy(job.dbname, dbtable, table - dbtable);
	else
		strcpy(job.dbname, dbtable);
	//look for the configuration on the KB config file
	if (!table || !*config) 
	{
		char config_path[LDB_MAX_PATH] = "";
		sprintf(config_path,"%s%s.conf", LDB_CFG_PATH,job.dbname);
		if (!ldb_file_exists(config_path))
		{
			fprintf(stderr, "Warning, %s does not exist, creating default\n", config_path);
			if (!ldb_create_db_config_default(job.dbname))
				ldb_error("Error creating ldb default config\n");
		}
	}
	//Parse the stdin configuration
	if (*config)
	{
		ldb_importation_config_parse(&user_opt, config);
		if (user_opt.params.verbose > 0)
			logger_set_level(user_opt.params.verbose);
	}

	if (!ldb_database_exists(job.dbname))
		ldb_create_database(job.dbname);
	
	struct ldb_importation_jobs_s jobs = {.job = NULL, .number = 0, .user_opt = &user_opt, .global_opt = &global_opt};
	strcpy(jobs.dbname, job.dbname);
	jobs.unsorted_index = 0;
	for (int i = 0; i < LDB_DEFAULT_TABLES_NUMBER; i++)
	{
		jobs.sorted[i] = -1;
		jobs.unsorted[i] = -1;
	}

	if (!table && ldb_dir_exists(path))
	{
		strcpy(job.path, path);
			//check if the version file is present and update the kb version.
		bool version_present = version_import(&job);

		recurse_directory(&jobs, path, NULL);
		opt_add(jobs.user_opt, jobs.global_opt);
		opt_add(jobs.global_opt, jobs.user_opt);

		//abort the job if VERSION_VALIDATION is active and the json file is not present
		if (job.opt.params.version_validation && !version_present)
		{
			fprintf(stderr, "Failed to validate version.json, check if it is present in %s and it has the correct format\n", job.path);
			fprintf(stderr, "You can avoid this error by setting \"VALIDATE_VERSION = 0\" in the import configuration options\n");
			exit(EXIT_FAILURE);
		}

		print_jobs(&jobs);
		max_threads = jobs.user_opt->params.threads;
		fprintf(stderr, "Max threads set to: %d\n", max_threads);
		pthread_mutex_init(&lock, NULL);
		threads_list = calloc(max_threads, sizeof(pthread_t));
		logger_init(job.dbname, max_threads, threads_list);
		log_table_config("GLOBAL", jobs.user_opt);
		/* Process jobs*/
		for (int i=0; i < LDB_DEFAULT_TABLES_NUMBER; i++)
		{
			int lines_to_add = 0;
			if (jobs.sorted[i] != -1)
			{
				if (*jobs.job[jobs.sorted[i]]->csv_path)
					lines_to_add = 1;
				else
					lines_to_add = max_threads;
				
				log_table_config(jobs.job[jobs.sorted[i]]->table, &jobs.job[jobs.sorted[i]]->opt);
				logger_basic("%s",jobs.job[jobs.sorted[i]]->table);
				process_sectors(jobs.job[jobs.sorted[i]], threads_list);
				free(jobs.job[jobs.sorted[i]]);
				//wait for each table to finish
				threads_end(threads_list);
				logger_offset_increase(lines_to_add);
			}
		}
	
		for (int i=0; i < LDB_DEFAULT_TABLES_NUMBER; i++)
		{
			int lines_to_add = 0;
			if (jobs.unsorted[i] != -1)
			{
				if (*jobs.job[jobs.unsorted[i]]->csv_path)
					lines_to_add = 1;
				else
					lines_to_add = max_threads;
				
				log_table_config(jobs.job[jobs.unsorted[i]]->table, &jobs.job[jobs.unsorted[i]]->opt);
				logger_basic("%s",jobs.job[jobs.unsorted[i]]->table);
				process_sectors(jobs.job[jobs.unsorted[i]], threads_list);
				free(jobs.job[jobs.unsorted[i]]);
				//wait for each table to finish
				threads_end(threads_list);
				logger_offset_increase(lines_to_add);
			}
		}

		free(jobs.job);
	}
	else if (table)
	{
		strcpy(job.table, table + 1);
		if (ldb_file_exists(path))
		{
			strcpy(job.csv_path,path);
			strcpy(job.path, dirname(path));
		}
		else
		{
			strcpy(job.path,path);
		}

		load_import_config(&job, &global_opt);
		opt_add(&user_opt, &global_opt);
		opt_add(&user_opt, &job.opt);

		bool version_present = version_import(&job);
		//abort the job if VERSION_VALIDATION is active and the json file is not present
		if (job.opt.params.version_validation && !version_present)
		{
			fprintf(stderr, "Failed to validate version.json, check if it is present in %s and it has the correct format\n", job.path);
			fprintf(stderr, "You can avoid this error by setting \"VALIDATE_VERSION = 0\" in the import configuration options\n");
			exit(EXIT_FAILURE);
		}

		max_threads = job.opt.params.threads;
		fprintf(stderr, "Max threads set to: %d\n", max_threads);
		pthread_mutex_init(&lock, NULL);
		threads_list = calloc(max_threads, sizeof(pthread_t));
		logger_init(job.dbname, max_threads, threads_list);
		log_table_config(job.table, &job.opt);
		process_sectors(&job, threads_list);
	}
	else
	{
		ldb_error("Command error\n");
	}

	threads_end(threads_list);
	free(threads_list);
	pthread_mutex_destroy(&lock);

	return true;
}
