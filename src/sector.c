// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/sector.c
 *
 * LDB sector handling routines
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
#include "./ldb.h"
#include <sys/file.h> 
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "ldb_string.h"
/**
  * @file sector.c
  * @date 12 Jul 2020
  * @brief Contains functions related to table and database creations. Also has helper functions for sector handling.
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/sector.c
  */

FILE *lock_file(const char *filename, int wait_s, const char *mode)
{

	int fd = open(filename, O_RDWR);

	if (fd == -1)
	{
		perror("open");
		return NULL;
	}

	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	int ret;
	time_t start_time = time(NULL);
	while ((ret = fcntl(fd, F_SETLK, &fl)) == -1)
	{
		if (errno != EACCES && errno != EAGAIN)
		{
			perror("fcntl");
			close(fd);
			return NULL;
		}

		if (difftime(time(NULL), start_time) >= wait_s)
		{
			fprintf(stderr, "Timeout waiting for lock\n");
			close(fd);
			return NULL;
		}

		usleep(250); // Wait 1 millisecond before trying again
	}

	FILE *fp = fdopen(fd, mode);

	if (fp == NULL)
	{
		perror("fdopen");
		close(fd);
		//return NULL;
		exit(1);
	}
	return fp;
}

int ldb_close_unlock(FILE *fp) 
{
    int fd = fileno(fp);
    struct flock fl;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    int ret = fcntl(fd, F_SETLK, &fl);
    if (ret == -1) 
	{
        perror("fcntl");
        return -1;
    }

    if (fclose(fp) == EOF) 
	{
        perror("fclose");
        return -1;
    }
    return 0;
}
/**
 * @brief Opens an LDB sector and returns the file descriptor. If read mode, returns NULL
 *  in case it does not exist. Otherwise an empty sector file is created in case it
 *  does not exist
 * 
 * @param table Struct with the table configuration.
 * @param key Key of the table
 * @param mode Opens the db in read or write mode
 * @return FILE* File descriptor of the ldb_sector (sector_path is the filepath associated with a pair tablename and a key)
 */
FILE *ldb_open(struct ldb_table table, uint8_t *key, char *mode) {

	/* Create sector (file) if it doesn't already exist */
	char *sector_path = ldb_sector_path(table, key, mode);
	if (!sector_path) 
		return NULL;

	FILE *out = NULL;
	
	/* Open data sector */
	if (strcmp(mode, "r"))
		out = lock_file(sector_path, 5, mode);
	else
		out = fopen(sector_path, mode);
	
	if (!out) 
		fprintf(stderr, "Cannot open LDB with mode %s: %s\n", mode, strerror(errno));	

	free(sector_path);
	return out;
}

bool ldb_close(FILE * sector)
{
	if (fclose(sector) != 0)
	{
		printf("error closing sector\n");
		return false;
	}

	return true;
}


/**
 * @brief Creates a table in the ldb directory
 * 
 * @param db database name
 * @param table table name
 * @param keylen length of the key
 * @param reclen length of the record
 * @return true success. false failure
 */
bool ldb_create_table_new(char *db, char *table, int keylen, int reclen, int keys, int definitions)
{
	bool out = false;

	char *dbpath = malloc(LDB_MAX_PATH);
	sprintf(dbpath, "%s/%s", ldb_root, db);

	char *tablepath = malloc(LDB_MAX_PATH);
	sprintf(tablepath, "%s/%s/%s", ldb_root, db, table);

	if (keys < 1)
		keys = 1;

	if (!ldb_valid_name(db) || !ldb_valid_name(table))
	{
		printf("E064 Invalid characters or name is too long\n");
	}
	else if (!ldb_dir_exists(dbpath))
	{
		printf("E062 Database does not exist\n");
	}
	else if (ldb_dir_exists(tablepath))
	{
		printf("E069 Table already exists\n");
	}
	else {
		mkdir(tablepath, 0755);
		if (ldb_dir_exists(tablepath))
		{
			ldb_write_cfg(db, table, keylen, reclen, keys, definitions);
			out = true;
		}
		else printf("E065 Cannot create %s\n", tablepath);
	}

	free(dbpath);
	free(tablepath);
	return out;
}

//For backward compatibility
bool ldb_create_table(char *db, char *table, int keylen, int reclen)
{
	return ldb_create_table_new(db, table, keylen, reclen, 1, 0);
}

/**
 * @brief Creates the databases folders from a database name
 * The path for the folder is a concatenation of ldb_root + database name. ldb_root is defined in ldb.c
 * 
 * @param database String with the database name
 * @return true Success
 */
bool ldb_create_database(char *database)
{
	bool out = false;

	char *path = malloc(LDB_MAX_PATH);
	sprintf(path, "%s/%s", ldb_root, database);
	if (ldb_dir_exists(path))
	{
		printf("E068 Database already exists\n");
	}
	else {
		mkdir(path, 0755);
		if (ldb_dir_exists(path))
			out = true;
		else
			printf("E065 Cannot create %s\n", path);
	}

	free(path);
	return out;
}


/**
 * @brief Loads an entire LDB sector into memory and returns a pointer
   (NULL if the sector does not exist)
 * 
 * @param table  Instance of the table struct.
 * @param key   Key of the sector to load.
 * @return uint8_t* Pointer to the block of memory with the sector loaded.
 */
uint8_t *ldb_load_sector(struct ldb_table table, uint8_t *key) {

	FILE *ldb_sector = ldb_open(table, key, "r");
	if (!ldb_sector) return NULL;

	fseeko64(ldb_sector, 0, SEEK_END);
	uint64_t size = ftello64(ldb_sector);

	uint8_t *out = malloc(size);
	if (!out)
		 return NULL;
		 
	fseeko64(ldb_sector, 0, SEEK_SET);
	if (!fread(out, 1, size, ldb_sector)) printf("Warning: ldb_load_sector failed\n");
	fclose(ldb_sector);

	return out;
}


/**
 * @brief Loads an entire LDB sector into memory and returns a pointer
   (NULL if the sector does not exist)
 * 
 * @param table  Instance of the table struct.
 * @param key   Key of the sector to load.
 * @return uint8_t* Pointer to the block of memory with the sector loaded.
 */
ldb_sector_t ldb_load_sector_v2(struct ldb_table table, uint8_t *key) {

	ldb_sector_t sector = {.data = NULL, .id = *key, .size = 0};
	FILE *ldb_sector = ldb_open(table, key, "r");
	
	if (!ldb_sector) 
		return sector;

	fseeko64(ldb_sector, 0, SEEK_END);
	uint64_t size = ftello64(ldb_sector);

	uint8_t *out = malloc(size);
	if (!out)
		 return sector;
		 
	fseeko64(ldb_sector, 0, SEEK_SET);
	if (!fread(out, 1, size, ldb_sector)) 
	{
		out = NULL;
	}
	fclose(ldb_sector);
	sector.data = out;
	sector.size = size;
	return sector;
}

/**
 * @brief Reserves memory for storing a copy of an entire LDB sector
 * (returns NULL if the source sector does not exist)
 * 
 * @param table Instance of the table struct.
 * @param key Key of the sector
 * @return uint8_t* Pointer to the block of memory.
 */
uint8_t *ldb_load_new_sector(struct ldb_table table, uint8_t *key) {

	FILE *ldb_sector = ldb_open(table, key, "r");	// Opens a ldb in read mode. Will return NULL if it does not exist.
	if (!ldb_sector) return NULL;

	fseeko64(ldb_sector, 0, SEEK_END);
	uint64_t size = ftello64(ldb_sector);
	fclose(ldb_sector);

	if (!size) return NULL;

	uint8_t *out = malloc(size);
	return out;
}

/**
 * @brief Create an empty data sector (empty map)
 * 
 * @param sector_path Path to the sector
 */
void ldb_create_sector(char *sector_path)
{
	uint8_t *ldb_empty_map = calloc(LDB_MAP_SIZE, 1);

	FILE *ldb_map = fopen(sector_path, "w");
	if (!ldb_map)
	{
		ldb_error("E065 Cannot access ldb table. Check permissions.");
		exit(EXIT_FAILURE);
	}
	fwrite(ldb_empty_map, LDB_MAP_SIZE, 1, ldb_map);
	fclose(ldb_map);

	free(ldb_empty_map);
}

/**
 * @brief Moves sector.tmp into sector.ldb
 * Copy a temporary sector into a permanent sector.
 * 
 * @param table Instance of the table struct.
 * @param key Key of the sector.
 */
void ldb_sector_update(struct ldb_table table, uint8_t *key)
{
	char sector_ldb[LDB_MAX_PATH] = "\0";
	char sector_tmp[LDB_MAX_PATH] = "\0";
	sprintf(sector_ldb, "%s/%s/%s/%02x.ldb", ldb_root, table.db, table.table, key[0]);
	sprintf(sector_tmp, "%s/%s/%s/%02x.tmp", ldb_root, table.db, table.table, key[0]);

	if (!ldb_file_exists(sector_tmp))
	{
		printf("%s\n", sector_tmp);
		ldb_error("E074 Cannot update sector with .tmp ");
	}

	if (ldb_file_exists(sector_ldb) && unlink(sector_ldb))
		ldb_error("E074 Cannot update sector with .tmp, cannot remove old .ldb file.");
	
	if (!rename(sector_tmp, sector_ldb)) 
		return;

	ldb_error("E074 Error replacing sector with .tmp");
}

/**
 * @brief Erases sector.ldb
 * 
 * Does not erase sector.tmp
 * 
 * @param table Table struct that will be erased
 * @param key Key of the sector to be erased
 */
void ldb_sector_erase(struct ldb_table table, uint8_t *key)
{
	char sector_ldb[LDB_MAX_PATH] = "\0";
	sprintf(sector_ldb, "%s/%s/%s/%02x.ldb", ldb_root, table.db, table.table, key[0]);

	if (!ldb_file_exists(sector_ldb))
	{
		ldb_error("E074 Cannot erase sector");
	}

	if (!unlink(sector_ldb)) return;

	ldb_error("E074 Error erasing sector");
}

/**
 * @brief Returns the sector path for a given table_path and key
 * 
 * Table_path is a concatenation of ldb_root + database_name + table_name
 * 	- ldb_root is defined on ldb.c
 * 	- database_name is obtained from the struct table (table.db)
 *  - table_name is obtained from the struct table (table.table)
 * 
 * @param table Instance of the table struct in order to get the sector_path
 * @param key Key of the sector to be accessed
 * @param mode Mode of the sector to be accessed mode = "r" for only read. mode = "r+" for reads and write
 * @param tmp Boolean to indicate if the sector is temporary or not. (Will return the path with extention .tmp or .ldb)
 * @return char* Sector path according to the table and key provided.
 */
char *ldb_sector_path(struct ldb_table table, uint8_t *key, char *mode)
{
	/* Create table (directory) if it doesn't already exist */
	char table_path[LDB_MAX_PATH] = "\0";
	sprintf (table_path, "%s/%s/%s", ldb_root, table.db, table.table);

	if (!ldb_dir_exists(table_path))
	{
		printf("E063 Table %s does not exist\n", table_path);
		exit(EXIT_FAILURE);
	}

	char *sector_path = malloc(strlen(table_path) + LDB_MAX_PATH + 1);
	if (table.tmp)
		sprintf(sector_path, "%s/%02x.tmp", table_path, key[0]);
	else
		sprintf(sector_path, "%s/%02x.ldb", table_path, key[0]);

	/* If opening a tmp table, we remove the file if it exists */
	//if (ldb_file_exists(sector_path) && tmp) remove(sector_path);

	if (!ldb_file_exists(sector_path))
	{
		if (!strcmp(mode,"r"))
		{
			free(sector_path);
			return NULL;
		}
		ldb_create_sector(sector_path);
	}

	return sector_path;
}