// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/mz_optimise.c
 *
 * SCANOSS .mz optimisation functions
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
  * @file mz_optimize.c
  * @date 26 Oct 2021 
  * @brief Implement MZ optimization functions
  */

#include <zlib.h>
#include <libgen.h>
#include "ldb.h"
#include <string.h>
#include "mz.h"
#include "logger.h"
#include "collate.h"
#define MAX_FILE_SIZE (4 * 1048576)

/**
 * @brief Check if job->id is found in job->xkeys (see -X)
 * 
 * @param job pointer to mz job
 * @return true if it was found
 */
bool mz_id_excluded(struct mz_job *job)
{
	if (!job->xkeys_ln) return false;

	for (uint64_t i = 0; i < job->xkeys_ln; i += MD5_LEN)
	{
		/* Compare job id (bytes 3-16) */
		if (!memcmp(job->xkeys + i + 2, job->id, MD5_LEN - 2))
		{
			/* Compare mz id (bytes 1-2) */
			if (!memcmp(job->xkeys + i, job->mz_id, 2)) return true;
		}
	}

	return false;
}

bool mz_optimise_dup_handler(struct mz_job *job)
{
	/* Check if file is not duplicated */
	if (mz_id_excluded(job))
	{
		job->exc_c++;
	}
	else if (mz_id_exists(job->ptr, job->ptr_ln, job->id))
	{
		job->dup_c++;
	}
	else
	{
		memcpy(job->ptr + job->ptr_ln, job->id, job->ln);
		job->ptr_ln += job->ln;
	}

	return true;
}


/**
 * @brief Optimise an mz file removing duplicated data
 * 
 * @param job pointer to mz job
 */
void mz_collate(struct mz_job *job)
{
	/* Extract first two MD5 bytes from the file name */
	memcpy(job->md5, basename(job->path), 4);
	ldb_hex_to_bin(job->md5, 4, job->mz_id);

	/* Read source mz file into memory */
	job->mz = file_read(job->path, &job->mz_ln);

	/* Reserve memory for destination mz */
	job->ptr = calloc(job->mz_ln, 1);

	/* Launch optimisation */

	mz_parse(job, mz_optimise_dup_handler);
	
	/* Write updated mz file */
	file_write(job->path, job->ptr, job->ptr_ln);

	if (job->dup_c) 
		log_info("%s: %u duplicated files eliminated\n", job->path, job->dup_c);
	
	if (job->exc_c) 
		log_info("%s: %u keys excluded\n", job->path, job->exc_c);

	free(job->mz);
	free(job->ptr);
}


void ldb_collate_mz_table(struct ldb_table table, int p_sector, job_delete_tuples_t * delete)
{
	/* Start with sector 0*/
	uint16_t k0 = 0;
	logger_dbname_set(table.db);
	
	if (p_sector >= 0)
	{
		k0 = p_sector;
	}

	if (delete)
		k0 = *delete->tuples[0]->key;

	do {
		char sector_path[LDB_MAX_PATH] = "\0";
		sprintf(sector_path, "%s/%s/%s/%.4x.mz", ldb_root, table.db, table.table, k0);
		bool file_exist = ldb_file_exists(sector_path);
		if (!file_exist) //check for encoded files
		{
			strcat(sector_path, ".enc");
			file_exist = ldb_file_exists(sector_path);
		}
		if (file_exist)
		{
			log_info("Processing %s (remove duplicates)\n", sector_path);
			char *src = calloc(MAX_FILE_SIZE + 1, 1);
			uint8_t *zsrc = calloc((MAX_FILE_SIZE + 1) * 2, 1);

			struct mz_job job;
			*job.path = 0;
			memset(job.mz_id, 0, 2);
			job.mz = NULL;
			job.mz_ln = 0;
			job.id = NULL;
			job.ln = 0;
			job.data = src;        // Uncompressed data
			job.data_ln = 0;
			job.zdata = zsrc;      // Compressed data
			job.zdata_ln = 0;
			job.ptr = NULL;        // Temporary data
			job.ptr_ln = 0;
			job.dup_c = 0;
			job.igl_c = 0;
			job.orp_c = 0;
			job.min_c = 0;
			job.md5[32] = 0;
			job.check_only = false;
			job.dump_keys = false;
			job.orphan_rm = false;
			job.key = NULL;
			job.xkeys = NULL;
			job.xkeys_ln = 0;
			job.licenses = NULL;
			job.license_count = 0;
			job.exc_c = 0;
			job.dup_c = 0;
			strcpy(job.path, sector_path);

			if (delete)
			{
				job.xkeys = calloc(delete->tuples_number, MD5_LEN);
				job.xkeys_ln = delete->tuples_number * MD5_LEN;
				for (int i = 0; i < delete->tuples_number; i++)
				{
					memcpy(job.xkeys + i * MD5_LEN, delete->tuples[i]->key, MD5_LEN);
				}
			}
			
			mz_collate(&job);
			
			free(src);
			free(zsrc);
			free(job.xkeys);
		}

		if (p_sector >=0)
			break;

	
	} while (k0++ < 0xffff);

}


