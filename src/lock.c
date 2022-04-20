// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/lock.c
 *
 * DB Locking mechanisms
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
  * @file lock.c
  * @date 12 Jul 2020 
  * @brief Contains LDB lock functions
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/lock.c
  */

/**
 * @brief Verifies if the db is locked.
 * Reads the file ldb.lock and if exists the db is locked. Otherwise is free to use.
 * 
 * @return true if the db is locked, false otherwise.
 */
bool ldb_locked(char * db_name)
{
	char file_lock[LDB_MAX_PATH];
	sprintf(file_lock,"%s.%s", ldb_lock_path, db_name);
	return ldb_file_exists (file_lock);
}

/**
 * @brief Lock LDB for writing
 * 
 */
void ldb_lock(char * db_table)
{
	pid_t pid = getpid();
	char file_lock[LDB_MAX_PATH];

	char * table_name = strrchr(db_table,'/');
	
	if (!table_name)
		table_name = db_table;
	else
		table_name++;

	sprintf(file_lock,"%s.%s", ldb_lock_path, table_name);
	
	if (ldb_locked(db_table)) ldb_error ("E051 Concurrent ldb writing not supported (/dev/shm/ldb.lock exists)");

	/* Write lock file */
	FILE *lock = fopen (file_lock, "wb");

	if (!lock)
	{
		printf("Failed to create lock file %s\n", file_lock);
		exit(1);
	}

	if (!fwrite (&pid, 4, 1, lock)) printf("Warning: cannot write lock file\n");
	fclose (lock);

	/* Validate lock file */
	lock = fopen (file_lock, "rb");
	if (!fread (&pid, 4, 1, lock)) printf("Warning: cannot read lock file\n");
	fclose (lock);

	if (pid != getpid()) ldb_error ("E052 Concurrent ldb writing is not supported. (check /dev/shm/ldb.lock)");
}

/**
 * @brief Unlock LDB 
 * 
 */
void ldb_unlock(char * db_table)
{
	char file_lock[LDB_MAX_PATH];
	sprintf(file_lock,"%s.%s", ldb_lock_path, db_table);
	unlink(file_lock);
}

