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
  * @brief // TODO
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/collate.c
  */

/**
 * @brief Loads table configuration from .cfg file
 * 
 * @param db // TODO
 * @param table // TODO
 * @param rs // TODO
 * @return true // TODO
 * @return false // TODO
 */
bool ldb_load_cfg(char *db, char *table, struct ldb_recordset *rs)
{
	char *path = malloc(LDB_MAX_PATH);

	// Open configuration file
	sprintf(path, "%s/%s/%s.cfg", ldb_root, db, table);
	FILE *cfg = fopen(path, "r");
	free(path);

	if (!cfg) return false;

	// Read configuration file
	char *buffer = malloc(LDB_MAX_NAME);
	if (!fread(buffer, 1, LDB_MAX_NAME, cfg)) printf("Warning: cannot open file %s\n", path);
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
	return true;
}

/**
 * @brief // TODO
 * 
 * @param db_table // TODO
 * @return struct ldb_table // TODO
 */
struct ldb_table ldb_read_cfg(char *db_table)
{
	struct ldb_table tablecfg;
	char *path = malloc(LDB_MAX_PATH);
	
	// Open configuration file
	sprintf(path, "%s/%s.cfg", ldb_root, db_table);
	FILE *cfg = fopen(path, "r");
	free(path);

	memcpy(tablecfg.db,   "\0", 1);
	memcpy(tablecfg.table, "\0", 1);
	tablecfg.tmp = false;

	if (cfg != NULL) {

		// Read configuration file
		char *buffer = calloc(LDB_MAX_NAME, 1);
		if (!fread(buffer, 1, LDB_MAX_NAME, cfg)) printf("Warning: cannot read file %s\n", path);
		char *reclen = buffer + ldb_split_string(buffer, ',');

		// Assign values to cfg structure
		char tmp[LDB_MAX_PATH] = "\0";
		strcpy(tmp, db_table);
		char *tablename = tmp + ldb_split_string(tmp, '/');
		strcpy(tablecfg.db, tmp);
		strcpy(tablecfg.table, tablename);
		tablecfg.key_ln = atoi(buffer);
		tablecfg.rec_ln = atoi(reclen);
		tablecfg.ts_ln = 2;
		fclose(cfg);
		free(buffer);
	}

	return tablecfg;
}

/**
 * @brief // TODO
 * 
 * @param db // TODO
 * @param table // TODO
 * @param keylen // TODO
 * @param reclen // TODO
 */
void ldb_write_cfg(char *db, char *table, int keylen, int reclen)
{
	char *path = malloc(LDB_MAX_PATH);
	sprintf(path, "%s/%s/%s.cfg", ldb_root, db, table);

	FILE *cfg = fopen(path, "w");
	fprintf(cfg,"%d,%d\n", keylen, reclen);
	fclose(cfg);

	free(path);
}

