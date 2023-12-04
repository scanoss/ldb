// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/config.c
 *
 * Configuration file handling routines
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
  * @file config.c
  * @date 12 Jul 2020 
  * @brief LDB configuration functions
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/collate.c
  */
#include "ldb.h"
#include "ldb_string.h"
#include "logger.h"
/**
 * @brief Loads table configuration from .cfg file
 * 
 * @param db DB string name
 * @param table table string name
 * @param rs ldb recordset pointer
 * @return true if succed
 */
bool ldb_load_cfg(char *db, char *table, struct ldb_recordset *rs)
{
	char *path = malloc(LDB_MAX_PATH);

	// Open configuration file
	sprintf(path, "%s/%s/%s.cfg", ldb_root, db, table);
	FILE *cfg = fopen(path, "r");

	if (!cfg) 
	{
		free(path);
		return false;
	}

	// Read configuration file
	char *buffer = malloc(LDB_MAX_NAME);
	if (!fread(buffer, 1, LDB_MAX_NAME, cfg)) 
		printf("Warning: cannot open file %s\n", path);
	fclose(cfg);
	char *reclen = buffer + ldb_split_string(buffer, ',');

	// Read values
	int key_ln = atoi(buffer);
	int rec_ln = atoi(reclen);
	// Validate values
	if (key_ln < 4 || key_ln > 255) return false;
	if (rec_ln < 0 || rec_ln > 255) return false;

	// Load values into recordset
	rs->key_ln = key_ln;
	rs->rec_ln = rec_ln;
	rs->subkey_ln = key_ln - 4;
	strcpy(rs->db, db);
	strcpy(rs->table, table);

	free(buffer);
	free(path);
	return true;
}

/**
 * @brief Read table config from a file and loads insto a ldb_table structure
 * 
 * @param db_table DB table name
 * @return struct with table configuration
 */
struct ldb_table ldb_read_cfg(char *db_table)
{
	struct ldb_table tablecfg = {.db = "\0", .table = "\0", .key_ln = 16, .rec_ln = 0, .keys = 1, .tmp = false, .ts_ln = 2, .definitions = LDB_TABLE_DEFINITION_UNDEFINED}; // default config

	char tmp[LDB_MAX_PATH] = "\0";
	strcpy(tmp, db_table);
	char *tablename = tmp + ldb_split_string(tmp, '/');

	strcpy(tablecfg.db, tmp);
	strcpy(tablecfg.table, tablename);

	char path[LDB_MAX_PATH] = "\0";

	// Open configuration file
	sprintf(path, "%s/%s.cfg", ldb_root, db_table);
	FILE *cfg = fopen(path, "r");

	if (!cfg)
	{
		log_info("Warning: config file \"%s\" does not exist. Using table's default config\n", path);
		return tablecfg;
	}

	// Read configuration file
	int key_ln, rec_ln, keys, definitions;
	int result = fscanf(cfg, "%d,%d,%d,%d", &key_ln, &rec_ln, &keys, &definitions);

	if (result < 2)
	{
		log_info("Warning: cannot read file %s\n, using default config\n", path);
		fclose(cfg);
		return tablecfg;
	}

	tablecfg.key_ln = key_ln;
	tablecfg.rec_ln = rec_ln;

	// backward compatibility with cfg files
	if (result < 4)
	{
		log_info("Warning: some fields are undefined in config file %s, must be updated\n", path);
		keys = -1;
		definitions = -1;
	}

	tablecfg.keys = keys;
	tablecfg.definitions = definitions;
	fclose(cfg);
	return tablecfg;
}

/**
 * @brief Save db config into a file
 * 
 * @param db DB name
 * @param table Table name
 * @param keylen Key lenght
 * @param reclen register lenght
 */
void ldb_write_cfg(char *db, char *table, int keylen, int reclen, int keys, int definitions)
{
	char *path = malloc(LDB_MAX_PATH);
	sprintf(path, "%s/%s/%s.cfg", ldb_root, db, table);

	FILE *cfg = fopen(path, "w+");
	fprintf(cfg,"%d,%d,%d,%d\n", keylen, reclen, keys, definitions);
	fclose(cfg);

	free(path);
}

