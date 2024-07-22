// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/ldb.c
 *
 * LDB Database - A mapped linked-list database
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
  * @file ldb.c
  * @date 12 Jul 2020 
  * @brief LDB commands definition
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/keys.c
  */

#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ldb.h"


/* Global */
char ldb_root[] = "/var/lib/ldb";
char ldb_lock_path[] = "/dev/shm/ldb.lock";
int ldb_cmp_width = 0;

bool ldb_read_failure = false;
/**
 * @brief Display LDB error and exit program
 * 
 * @param txt error string
 */
void ldb_error (char *txt)
{
	fprintf(stdout, "%s\n", txt);
	exit(EXIT_FAILURE);
}

/**
 * @brief Print ldb version
 * 
 */
void ldb_version(char **version)
{
  if (!version)
    printf("ldb-%s\n", LDB_VERSION);
  else
    *version = strdup(LDB_VERSION);
}

