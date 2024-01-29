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
/**
  * @file mz.c
  * @date 7 Feb 2021 
  * @brief Implements MZ functions used to compress and decompress LDB blocks
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/lock.c
  */

#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>
#include "ldb.h"
#include "mz.h"
#include <gcrypt.h>
#include "logger.h"
/**
 * @brief compare two MZ keys
 * 
 * @param a key a
 * @param b key b
 * @return 1 if a>b, -1 if b>a, 0 if they are equals
 */
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

/**
 * @brief Handling function for listing mz keys 
 * 
 * @param job pointer to mz_job struct
 * @return true
 */
bool mz_dump_keys_handler(struct mz_job *job)
{
	/* Fill MD5 with item id */
	mz_id_fill(job->md5, job->id);

	ldb_hex_to_bin(job->md5, MD5_LEN * 2, job->ptr + job->ptr_ln);
	job->ptr_ln += MD5_LEN;

	return true;
}

/**
 * @brief Output unique mz keys to STDOUT (binary)
 * 
 * @param job input pointer to mz jon struct
 */
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

/**
 * @brief Handling function for listing mz contents in stdout
 * 
 * @param job pointer to mz input struct
 * @return true Always true
 */
bool mz_list_check_handler(struct mz_job *job)
{
	/* Fill MD5 with item id */
	mz_id_fill(job->md5, job->id);

	/* Decompress */
	mz_deflate(job);

	/* Calculate resulting data MD5 */
	uint8_t actual_md5[MD5_LEN];
	MD5( (unsigned char*) job->data, job->data_ln, actual_md5);

	/* Compare data checksum to validate */
	char actual[MD5_LEN * 2 + 1] = "\0";
	ldb_bin_to_hex(actual_md5, MD5_LEN, actual);

	if (strcmp(job->md5, actual))
	{
		mz_corrupted(0);
	}
	else if (!job->check_only)
	{
		printf("%s [OK] %lu bytes\n", job->md5, job->data_ln);
	}

	return true;
}

bool mz_list_handler(struct mz_job *job)
{
	/* Fill MD5 with item id */
	mz_id_fill(job->md5, job->id);
	printf("%s %lu bytes\n", job->md5, job->zdata_ln);

	return true;
}

/**
 * @brief 
 * 
 * @param job MZ job to be procesed
 */
void mz_list_check(struct mz_job *job)
{
	/* Extract first two MD5 bytes from the file name */
	memcpy(job->md5, basename(job->path), 4);

	/* Read source mz file into memory */
	job->mz = file_read(job->path, &job->mz_ln);

	/* List mz contents */
	if (!job->dump_keys) mz_parse(job, mz_list_check_handler);

	/* Dump mz keys */
	else mz_dump_keys(job);

	free(job->mz);
}

/**
 * @brief 
 * 
 * @param job MZ job to be procesed
 */
void mz_list_keys(struct ldb_table table, int sector)
{
	char sector_path[LDB_MAX_PATH] = "\0";
	
	for(int k = sector >= 0 ? sector : 0; k < 0xffff; k++)
	{
		sprintf(sector_path, "%s/%s/%s/%.4x.mz", ldb_root, table.db, table.table, k);
		bool file_exist = ldb_file_exists(sector_path);
		if (!file_exist) //check for encoded files
		{
			strcat(sector_path, ".enc");
			file_exist = ldb_file_exists(sector_path);
		}

		if (file_exist)
		{
			struct mz_job job;
			strcpy(job.path, sector_path);

			/* Extract first two MD5 bytes from the file name */
			memcpy(job.md5, basename(job.path), 4);

			/* Read source mz file into memory */
			job.mz = file_read(job.path, &job.mz_ln);
			mz_parse(&job, mz_list_handler);
			free(job.mz);
		}

		if (sector >= 0)
			break;
	}
}

/**
 * @brief Handler to find if a key exist in the mz_job struct, in that case job.key will be true.
 * 
 * @param job pointer to input mz job struct
 * @return true if key exist, false otherwise.
 */
bool mz_key_exists_handler(struct mz_job *job)
{
	if (!memcmp(job->id, job->key + 2, MZ_MD5))
	{
		job->key_found = true;
		return false;
	}
	return true;
}

/**
 * @brief Handler for MZ cat operation
 * 
 * @param job pointer to input mz job
 * @return true to finish
 * @return false to continue
 */
bool mz_cat_handler(struct mz_job *job)
{
	if (!memcmp(job->id, job->key + 2, MZ_MD5))
	{
		/* Decrypt (if encrypted) */
		if (job->decrypt)
			job->decrypt(job->id, job->zdata_ln);
		/* Decompress */
		mz_deflate(job);

		job->data[job->data_ln] = 0;
		printf("%s", job->data);

		return false;
	}
	return true;
}

/**
 * @brief Find a key inside a mz file
 * 
 * @param job input mz job
 * @param key key to be found
 * @return true if the key exist
 */
bool mz_key_exists(struct mz_job *job, uint8_t *key)
{
	/* Calculate mz file path */
	char mz_path[LDB_MAX_PATH + MD5_LEN] = "\0";
	char mz_file_id[5] = "\0\0\0\0\0";
	ldb_bin_to_hex(key, 2, mz_file_id);
	sprintf(mz_path, "%s/%s.mz", job->path, mz_file_id);

	/* Save path and key on job */
	job->key_found = false;
	job->key = key;

	/* Read source mz file into memory */
	job->mz = file_read(mz_path, &job->mz_ln);

	/* Search and display "key" file contents */
	mz_parse(job, mz_key_exists_handler);

	free(job->mz);

	return job->key_found;
}

/**
 * @brief Find a key and print the result
 * 
 * @param job input mz job
 * @param key key to be found
 */
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

	free(job->data);
	free(job->key);
	free(job->mz);
}

/**
 * @brief Handler to extract a mz file
 * 
 * @param job input mz job
 * @return true Always return true
 */
bool mz_extract_handler(struct mz_job *job)
{
	/* Fill MD5 with item id */
	mz_id_fill(job->md5, job->id);

	/* Decompress */
	mz_deflate(job);

	/* Calculate resulting data MD5 */
	uint8_t actual_md5[MD5_LEN];
	MD5((unsigned char*) job->data, job->data_ln, actual_md5);

	/* Compare data checksum to validate */
	char actual[MD5_LEN * 2 + 1] = "\0";
	ldb_bin_to_hex(actual_md5, MD5_LEN, actual);

	if (strcmp(job->md5, actual))
	{
		mz_corrupted(0);
	}
	else
	{
		printf("Extracting %s (%lu bytes)\n", job->md5, job->data_ln);

		/* Extract data to file */
		file_write(job->md5, (uint8_t *)job->data, job->data_ln);
	}

	return true;
}

/**
 * @brief Extract the content from a mz file
 * 
 * @param job pointer to mz job
 */
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

/**
 * @brief Read file and return pointer to file contents
 * 
 * @param filename file name string
 * @param size [out] file size
 * @return pointer to file content
 */
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
	if (1 != fread(tmp, *size, 1, f)) *tmp = 0;

	fclose(f);

	return tmp;
}

/**
 * @brief Write data to file
 * 
 * @param filename file name string
 * @param src pointer to data to be written
 * @param src_ln data size
 */
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

/**
 * @brief Searches for a 14-byte ID in *mz
 * 
 * @param mz pointer to mz content
 * @param size content size
 * @param id id to be found
 * @return true if the id exist
 */
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

/**
 * @brief Walks to an mz file, calling mz_parse_handler with each iteration
 * 
 * @param job mz input job
 * @param mz_parse_handler function pointer to be executed
 */
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
			fprintf(stderr, "%s integrity failed\n", job->path);
		}
	}
}

/**
 * @brief Create a uint16_t from two bytes.
 * 
 * @param data pointer to input data
 * @return uint16_t result
 */
static uint16_t uint16(uint8_t *data)
{
    return 256 * data[0] + data[1];
}

/**
 * @brief Returns true if an ID exists in the mz_cache
 * 
 * @param md5 id to be found
 * @param mz_cache pointer to mz cache data
 * @return true if exist
 */
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

/**
 * @brief Returns true if an mz ID exists in the mz archive
 * 
 * @param md5 ID to be found
 * @param mined_path path to the mz file
 * @return true if the ID exist
 */
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

/**
 * @brief Returns true if an mz ID exists on disk or in memory
 * 
 * @param mined_path path to the mz file
 * @param md5 ID to be found
 * @param mz_cache pointer to mz cache
 * @return true if the key exist
 */
bool mz_exists(char *mined_path, uint8_t *md5, struct mz_cache_item *mz_cache)
{
	if (mz_exists_in_cache(md5, mz_cache)) return true;

	return mz_exists_in_disk(md5, mined_path);
}

/**
 * @brief Appends mz_cache to the mz archive in disk 
 * 
 * @param mined_path mz file path
 * @param mzid mz ID
 * @param data data to be added
 * @param datalen data len
 */
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

/**
 * @brief Adds a file to the mz archive. File structure is a series of:
 *	 MD5(14) + COMPRESSED_SRC_SIZE(4) + COMPRESSED_SRC(N)
 *	 The first two bytes of the md5 are in the actual XXXX.mz filename
 * 
 * @param mined_path mz file path
 * @param md5 MD5 key
 * @param src pointer to source buffer
 * @param src_ln source buffer len
 * @param check true to check if the key exist
 * @param zsrc 
 * @param mz_cache pointer to mz cache
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

/**
 * @brief Write all cached mz records
 * 
 * @param mined_path path to mz file
 * @param mz_cache pointer to mz buffer
 */
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

/**
 * @brief Checks mz container for integrity
 * 
 * @param path mz file path
 * @return true if the test pass
 */
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

/**
 * @brief Fills bytes 3-16 of md5 with id 
 * 
 * @param md5[out] md5 output
 * @param mz_id mz ID
 */
void mz_id_fill(char *md5, uint8_t *mz_id)
{
	for (int i = 0; i < 14; i++)
	{
		sprintf(md5 + 4 + 2 * i, "%02x", mz_id[i]);
	}
}

/**
 * @brief Print corrupted error message
 * 
 */
void mz_corrupted( )
{
	printf("Corrupted mz file\n");
	exit(EXIT_FAILURE);
}

#define CHUNK_SIZE 1024

int uncompress_by_chunks(uint8_t **data, uint8_t *zdata, size_t zdata_len) {
    int ret;
    z_stream strm;
    unsigned char out[CHUNK_SIZE];
    size_t data_size = 0;  // Current size of decompressed data

    // Initialize the z_stream structure
    memset(&strm, 0, sizeof(strm));
    ret = inflateInit(&strm);
    if (ret != Z_OK) {
        fprintf(stderr, "inflateInit failed with error %d\n", ret);
        exit(EXIT_FAILURE);
    }
	*data = malloc(CHUNK_SIZE);
    // Process the compressed data
    strm.avail_in = zdata_len;  // Size of the compressed data
    strm.next_in = zdata;

    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = out;

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            fprintf(stderr, "inflate failed with error Z_STREAM_ERROR\n");
            inflateEnd(&strm);
			mz_corrupted();
        }

        unsigned have = CHUNK_SIZE - strm.avail_out;

        // Realloc to increase the size of data
        *data = realloc(*data, data_size + have);
        if (*data == NULL) 
		{
            fprintf(stderr, "Error reallocating memory to store decompressed data");
            inflateEnd(&strm);
            exit(EXIT_FAILURE);
        }

        // Copy the decompressed data to the end of data
        memcpy(*data + data_size, out, have);
        data_size += have;
    } while (ret != Z_STREAM_END);

    // Free resources
    inflateEnd(&strm);
	return data_size;
}

void mz_deflate(struct mz_job *job)
{
	/* Decompress data */
	job->data_ln = uncompress_by_chunks((uint8_t **) &job->data, job->zdata, job->zdata_ln);
	job->data_ln--;
}
