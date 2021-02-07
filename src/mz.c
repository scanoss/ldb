// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/mz.c
 *
 * SCANOSS .mz archive handling functions
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

/* Read data to memory and return a pointer. Also, update size */

#include <openssl/md5.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>
#include "ldb.h"

/* Returns the hexadecimal md5 sum for "path" */
uint8_t *file_md5 (char *path)
{
	uint8_t *c = calloc(16,1);
	FILE *fp = fopen(path, "rb");
	MD5_CTX mdContext;
	uint32_t bytes;

	if (fp != NULL)
	{
		uint8_t *buffer = malloc(BUFFER_SIZE);
		MD5_Init (&mdContext);

		while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) != 0)
			MD5_Update(&mdContext, buffer, bytes);

		MD5_Final(c, &mdContext);
		fclose(fp);
		free(buffer);
	}
	return c;
}

/* Calculates the MD5 hash for data */
void calc_md5(char *data, int size, uint8_t *out)
{
	MD5_CTX mdContext;
	MD5_Init (&mdContext);
	MD5_Update(&mdContext, data, size);
	MD5_Final(out, &mdContext);
}

int mz_key_cmp(const void * a, const void * b)
{
    const uint8_t *va = a;
    const uint8_t *vb = b;

    /* Compare byte by byte */
    for (int i = 0; i < MD5_LEN; i++)
    {
        if (va[i] > vb[i]) return 1;
        if (va[i] < vb[i]) return -1;
    }

    return 0;
}

/* Handling function for listing mz keys */
bool mz_dump_keys_handler(struct mz_job *job)
{
	/* Fill MD5 with item id */
	mz_id_fill(job->md5, job->id);

	ldb_hex_to_bin(job->md5, MD5_LEN * 2, job->ptr + job->ptr_ln);
	job->ptr_ln += MD5_LEN;

	return true;
}

/* Output unique mz keys to STDOUT (binary) */
void mz_dump_keys(struct mz_job *job)
{
	/* Use job->ptr to store keys */
	job->ptr = malloc(job->mz_ln);

	/* Fetch keys */
	mz_parse(job, mz_dump_keys_handler);

	/* Sort keys */
	qsort(job->ptr, job->ptr_ln / MD5_LEN, MD5_LEN, mz_key_cmp);

	/* Output keys */
	for (int i = 0; i < job->ptr_ln; i += 16)
	{
		bool skip = false;
		if (i) if (!memcmp(job->ptr + i, job->ptr + i - MD5_LEN, MD5_LEN))
		{
			skip = true;
		}
		if (!skip) fwrite(job->ptr + i, MD5_LEN, 1, stdout);
	}

	free(job->ptr);
}

/* Handling function for listing mz contents */
bool mz_list_handler(struct mz_job *job)
{
	/* Fill MD5 with item id */
	mz_id_fill(job->md5, job->id);

	/* Decompress */
	mz_deflate(job);

	/* Calculate resulting data MD5 */
	uint8_t actual_md5[MD5_LEN];
	calc_md5(job->data, job->data_ln, actual_md5);

	/* Compare data checksum to validate */
	char actual[MD5_LEN * 2 + 1] = "\0";
	ldb_bin_to_hex(actual_md5, MD5_LEN, actual);

	if (strcmp(job->md5, actual))
	{
		mz_corrupted();
	}
	else if (!job->check_only)
	{
		printf("%s [OK] %lu bytes\n", job->md5, job->data_ln);
	}

	return true;
}

void mz_list(struct mz_job *job)
{
	/* Extract first two MD5 bytes from the file name */
	memcpy(job->md5, basename(job->path), 4);

	/* Read source mz file into memory */
	job->mz = file_read(job->path, &job->mz_ln);

	/* List mz contents */
	if (!job->dump_keys) mz_parse(job, mz_list_handler);

	/* Dump mz keys */
	else mz_dump_keys(job);

	free(job->mz);
}

bool mz_cat_handler(struct mz_job *job)
{
	if (!memcmp(job->id, job->key + 2, MZ_MD5))
	{
		/* Decompress */
		mz_deflate(job);

		job->data[job->data_ln] = 0;
		printf("%s", job->data);

		return false;
	}
	return true;
}

void mz_cat(struct mz_job *job, char *key)
{
	/* Calculate mz file path */
	char mz_path[LDB_MAX_PATH + MD5_LEN] = "\0";
	char mz_file_id[5] = "\0\0\0\0\0";
	memcpy(mz_file_id, key, 4);

	sprintf(mz_path, "%s/%s.mz", job->path, mz_file_id);

	/* Save path and key on job */
	job->key = calloc(MD5_LEN, 1);
	ldb_hex_to_bin(key, MD5_LEN * 2, job->key);	

	/* Read source mz file into memory */
	job->mz = file_read(mz_path, &job->mz_ln);

	/* Search and display "key" file contents */
	mz_parse(job, mz_cat_handler);

	free(job->key);
	free(job->mz);
}

bool mz_extract_handler(struct mz_job *job)
{
	/* Fill MD5 with item id */
	mz_id_fill(job->md5, job->id);

	/* Decompress */
	mz_deflate(job);

	/* Calculate resulting data MD5 */
	uint8_t actual_md5[MD5_LEN];
	calc_md5(job->data, job->data_ln, actual_md5);

	/* Compare data checksum to validate */
	char actual[MD5_LEN * 2 + 1] = "\0";
	ldb_bin_to_hex(actual_md5, MD5_LEN, actual);

	if (strcmp(job->md5, actual))
	{
		mz_corrupted();
	}
	else
	{
		printf("Extracting %s (%lu bytes)\n", job->md5, job->data_ln);

		/* Extract data to file */
		file_write(job->md5, (uint8_t *)job->data, job->data_ln);
	}

	return true;
}

void mz_extract(struct mz_job *job)
{
	/* Extract first two MD5 bytes from the file name */
	memcpy(job->md5, basename(job->path), 4);

	/* Read source mz file into memory */
	job->mz = file_read(job->path, &job->mz_ln);

	/* Launch extraction */
	mz_parse(job, mz_extract_handler);

	free(job->mz);
}

/* Read file and return pointer to file contents */
uint8_t *file_read(char *filename, uint64_t *size)
{
	FILE *f = fopen(filename, "r");
	if (!f)
	{
		printf("\nCannot open %s for writing\n", filename);
		exit(EXIT_FAILURE);
	}

	fseeko64(f, 0, SEEK_END);
	*size = ftello64(f);
	fseeko64(f, 0, SEEK_SET);

	uint8_t *tmp = calloc(*size, 1);
	fread(tmp, *size, 1, f);
	fclose(f);

	return tmp;
}

/* Write data to file */
void file_write(char *filename, uint8_t *src, uint64_t src_ln)
{
	FILE *out = fopen(filename, "w");
	if (!out)
	{
		printf("\nCannot open %s for writing\n", filename);
		exit(EXIT_FAILURE);
	}
	fwrite(src, 1, src_ln, out);
	fclose(out);
}

/* Searches for a 14-byte ID in *mz */
bool mz_id_exists(uint8_t *mz, uint64_t size, uint8_t *id)
{
	/* Recurse mz contents */
	uint64_t ptr = 0;
	while (ptr < size)
	{
		/* Position pointers */
		uint8_t *file_id = mz + ptr;
		uint8_t *file_ln = file_id + MZ_MD5;

		/* Compare id */
		if (!memcmp(file_id, id, MZ_MD5)) return true;

		/* Get compressed data size */
		uint32_t tmpln;
		memcpy((uint8_t*)&tmpln, file_ln, MZ_SIZE);

		/* Increment pointer */
		ptr += tmpln + MZ_MD5 + MZ_SIZE;
	}
	return false;
}

/* Walks to an mz file, calling mz_parse_handler with each iteration */
void mz_parse(struct mz_job *job, bool (*mz_parse_handler) ())
{
	/* Recurse mz contents */
	uint64_t ptr = 0;
	while (ptr < job->mz_ln)
	{
		/* Position pointers */
		job->id = job->mz + ptr;
		uint8_t *file_ln = job->id + MZ_MD5;
		job->zdata = file_ln + MZ_SIZE;

		/* Get compressed data size */
		uint32_t tmpln;
		memcpy((uint8_t*)&tmpln, file_ln, MZ_SIZE);
		job->zdata_ln = tmpln;

		/* Get total mz record length */
		job->ln = MZ_MD5 + MZ_SIZE + job->zdata_ln;

		/* Pass job to handler */
		if (!mz_parse_handler(job)) return;

		/* Increment pointer */
		ptr += job->ln;
		if (ptr > job->mz_ln)
		{
			printf("%s integrity failed\n", job->path);
			exit(EXIT_FAILURE);
		}
	}
}

static uint16_t uint16(uint8_t *data)
{
    return 256 * data[0] + data[1];
}

/* Returns true if an ID exists in the mz_cache */
bool mz_exists_in_cache(uint8_t *md5, struct mz_cache_item *mz_cache)
{
	int mzid = uint16(md5);

	/* False if cache is empty */
	if (!mz_cache[mzid].length) return false;

	/* Search md5 in cache */
	uint8_t *cache = mz_cache[mzid].data;
	int cacheln = mz_cache[mzid].length;

	while (cache < (mz_cache[mzid].data + cacheln))
	{
		/* MD5 comparison starts on the third byte of the MD5 */
		if (!memcmp(md5 + 2, cache, MZ_MD5)) return true;

		/* Extract zsrc and displace cache pointer */
		uint32_t zsrc_ln;
		memcpy((uint8_t*)&zsrc_ln, cache + MZ_MD5, MZ_SIZE);
		cache += (MZ_HEAD + zsrc_ln);
	}

	return false;
}

/* Returns true if an mz ID exists in the mz archive */
bool mz_exists_in_disk(uint8_t *md5, char *mined_path)
{
	char path[LDB_MAX_PATH];

	/* Assemble MZ path */
	uint16_t mzid = uint16(md5);
	sprintf(path, "%s/sources/%04x.mz", mined_path, mzid);

	/* Open mz file */
	int mz = open(path, O_RDONLY);
	if (mz < 0) return false;

	uint64_t ptr = 0;
	uint8_t header[MZ_HEAD];

	/* Get file size */
	uint64_t size = lseek64(mz, 0, SEEK_END);
	if (!size) return false;

	/* Recurse file */
	while (ptr < size)
	{
		/* Read MZ header */
		lseek64(mz, ptr, SEEK_SET);
		if (!read(mz, header, MZ_HEAD))
		{
			printf("[READ_ERROR]\n");
			exit(EXIT_FAILURE);
		}
		ptr += MZ_HEAD;

		/* If md5 matches, exit with true */
		if (!memcmp(md5 + 2, header, MZ_MD5))
		{
			close(mz);
			return true;
		}

		/* Increment pointer */
		uint32_t zsrc_ln;
		memcpy((uint8_t*)&zsrc_ln, header + MZ_MD5, MZ_SIZE);
		ptr += zsrc_ln;
	}
	close(mz);

	return false;
}

/* Returns true if an mz ID exists on disk or in memory */
bool mz_exists(char *mined_path, uint8_t *md5, struct mz_cache_item *mz_cache)
{
	if (mz_exists_in_cache(md5, mz_cache)) return true;

	return mz_exists_in_disk(md5, mined_path);
}

/* Appends mz_cache to the mz archive in disk */
void mz_write(char *mined_path, int mzid, uint8_t *data, int datalen)
{
	char path[LDB_MAX_PATH];

	/* Create sources directory */
	sprintf(path, "%s/sources", mined_path);

	ldb_prepare_dir(path);

	sprintf(path, "%s/sources/%04x.mz", mined_path, mzid);
	FILE *f = fopen(path, "a");
	if (f)
	{
		size_t written = fwrite(data, datalen, 1, f);
		if (!written)
		{
			printf("Error writing %s\n", path);
			exit(EXIT_FAILURE);
		}
		fclose(f);
	}
}

/* Adds a file to the mz archive. File structure is a series of:
	 MD5(14) + COMPRESSED_SRC_SIZE(4) + COMPRESSED_SRC(N)
	 The first two bytes of the md5 are in the actual XXXX.mz filename
	 */
void mz_add(char *mined_path, uint8_t *md5, char *src, int src_ln, bool check, uint8_t *zsrc, struct mz_cache_item *mz_cache)
{
	if (check) if (mz_exists(mined_path, md5, mz_cache)) return;

	uint64_t zsrc_ln = compressBound(src_ln + 1);

	/* We save the first bytes of zsrc to accomodate the MZ header */
	compress(zsrc + MZ_HEAD, &zsrc_ln, (uint8_t *) src, src_ln + 1);
	uint32_t zln = zsrc_ln;

	/* Only the last 14 bytes of the MD5 go to the mz record (first two bytes are the file name) */
	memcpy(zsrc, md5 + 2, MZ_MD5); 

	/* Add the 32-bit compressed file length */
	memcpy(zsrc + MZ_MD5, (char *) &zln, MZ_SIZE);

	int mzid = uint16(md5);
	int mzlen = zsrc_ln + MZ_HEAD;

	/* If it won't fit in the cache, write it directly */
	if (mzlen > MZ_CACHE_SIZE)
	{
		mz_write(mined_path, mzid, zsrc, mzlen);
	}
	else
	{
		/* Flush cache and add to cache */
		if (mz_cache[mzid].length + mzlen > MZ_CACHE_SIZE)
		{
			mz_write(mined_path, mzid, zsrc, mzlen);
			mz_cache[mzid].length = mzlen;
			memcpy(mz_cache[mzid].data, zsrc, mzlen);
		}

		/* Just add to cache */
		else
		{
			memcpy(mz_cache[mzid].data + mz_cache[mzid].length, zsrc, mzlen);
			mz_cache[mzid].length += mzlen;
		}
	}
}

/* Write all cached mz records */
void mz_flush(char *mined_path, struct mz_cache_item *mz_cache)
{
	for (int i = 0; i < MZ_FILES; i++)
	{
		if (mz_cache[i].length)
		{
			mz_write(mined_path, i, mz_cache[i].data, mz_cache[i].length);
			mz_cache[i].length = 0;
		}
	}
}

/* Checks mz container for integrity */
bool mz_check(char *path)
{

	/* Open mz file */
	int mzfile = open(path, O_RDONLY);
	if (mzfile < 0) return false;

	/* Obtain file size */
	uint64_t size = lseek64(mzfile, 0, SEEK_END);
	if (!size) return false;

	/* Read entire .mz file to memory */
	uint8_t *mz = malloc(size);	
	lseek64 (mzfile, 0, SEEK_SET);
	if (!read(mzfile, mz, size)) 
	{
		free(mz);
		return false;
	}
	close(mzfile);

	/* Recurse mz contents */
	uint64_t ptr = 0;
	uint32_t ln = 0;

	while (ptr < size)
	{
		/* Read compressed data length */
		memcpy((uint8_t*)&ln, mz + ptr + MZ_MD5, MZ_SIZE);

		/* Increment ptr */
		ptr += (MZ_HEAD + ln);
	}
	close(mzfile);
	free(mz);

	if (ptr > size) return false;

	return true;
}

/* Fills bytes 3-16 of md5 with id */
void mz_id_fill(char *md5, uint8_t *mz_id)
{
	for (int i = 0; i < 14; i++)
	{
		sprintf(md5 + 4 + 2 * i, "%02x", mz_id[i]);
	}
}

void mz_corrupted()
{
	printf("Corrupted mz file\n");
	exit(EXIT_FAILURE);
}

void mz_deflate(struct mz_job *job)
{
	/* Decompress data */
	job->data_ln = MZ_MAX_FILE;
	if (Z_OK != uncompress((uint8_t *)job->data, &job->data_ln, job->zdata, job->zdata_ln))
	{
		mz_corrupted();
	}
	job->data_ln--;
}
