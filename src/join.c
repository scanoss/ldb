

/**
  * @file join.c
  * @date 14 Sep 2021 
  * @brief Contains functions used for implentent minr join funtionality
  */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <libgen.h>
#include "ldb.h"
#include "import.h"
#include "logger.h"
#include "ldb_error.h"

/**
 * @brief Creates a directory with 0755 permissions.
 * 
 * @param destination destination path
 */
void mkdir_if_not_exist(char *destination)
{
	char *dst_dir = strdup(destination);
	char *dir = dirname(dst_dir);
	if (ldb_dir_exists(dir))
	{
		free(dst_dir);
		return;
	}

	ldb_create_dir(dir);
	if (!ldb_dir_exists(dir))
	{
		printf("Cannot create directory %s\n", dst_dir);
		free(dst_dir);
		exit(EXIT_FAILURE);
	}

	free(dst_dir);
}

/**
 * @brief Move a file to a new location by copying byte by byte.
 * If the file already exist it is overwritten.
 * 
 * @param src src path
 * @param dst dst path 
 * @param skip_delete if true the src file is not deleted after the copy is done.
 * @return true success. False otherwise.
 */
static bool write_file(char *src, char *dst, char * mode, bool mkdir, bool delete) {
		
	if (mkdir)
	{
		mkdir_if_not_exist(dst);
	}
		
	FILE *srcf = fopen(src, "rb");
	if (!srcf)
	{	
		printf("Cannot open source file %s\n", src);
		exit(EXIT_FAILURE);
	}

	FILE *dstf = fopen(dst, mode);
	if (!dstf)
	{	
		printf("Cannot open destinstion file %s\n", dst);
		exit(EXIT_FAILURE);
	}

	/* Copy byte by byte */
	int byte = 0;
	while (!feof(srcf))
	{
		byte = fgetc(srcf);
		if (feof(srcf)) break;
		fputc(byte, dstf);
	}

	fclose(srcf);
	fclose(dstf);
	if (delete) unlink(src);
	return true;
}

bool move_file(char *src, char *dst, bool delete)
{
	return write_file(src,dst, "wb", true, delete);
}
/**
 * @brief Append the contents of a file to the end of another file.
 * 
 * @param file Origin of the data to be appended.
 * @param destination path to out file
 */

bool file_append(char *file, char *destination, bool delete)
{
	return write_file(file,destination, "ab", false, delete);
}



/**
 * @brief join two binary files
 * 
 * @param source path to the source file
 * @param destination path to destination file
 * @param snippets true if it is a snippet file
 * @param skip_delete true to avoid deletion
 */
int ldb_bin_join(char *source, char *destination, bool overwrite, bool snippets, bool delete)
{
	/* If source does not exist, no need to join */
	if (!ldb_file_exists(source)) 
	{
		fprintf(stderr,"Error: File %s does not exist\n", source);
		return -1;
	}

	if (ldb_file_exists(destination) && !overwrite)
	{
		/* Snippet records should divide by 21 */
		if (snippets) if (ldb_file_size(destination) % 21)
		{
			printf("File %s does not contain 21-byte records\n", destination);
			return -1;
		}
	}

	/* If destination does not exist. Source is moved */
	else
	{
		log_info("Moving %s into %s\n", source, destination);
		if (!move_file(source, destination, delete))
		{
			printf("Cannot move file\n");
			return -1;
		}
		return LDB_ERROR_NOERROR;
	}

	log_info("Joining into %s\n", destination);
	file_append(source, destination, delete);
	if (delete) unlink(source);

	return LDB_ERROR_NOERROR;
}


