

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

bool file_append(char *file, char *destination)
{
	return write_file(file,destination, "ab", false, true);
}



/**
 * @brief join two binary files
 * 
 * @param source path to the source file
 * @param destination path to destination file
 * @param snippets true if it is a snippet file
 * @param skip_delete true to avoid deletion
 */
bool ldb_bin_join(char *source, char *destination, bool snippets, bool delete)
{
	/* If source does not exist, no need to join */
	if (!ldb_file_exists(source)) 
	{
		fprintf(stderr,"Error: File %s does not exist\n", source);
		return false;
	}

	if (ldb_file_exists(destination))
	{
		/* Snippet records should divide by 21 */
		if (snippets) if (ldb_file_size(destination) % 21)
		{
			printf("File %s does not contain 21-byte records\n", destination);
			return false;
		}
	}

	/* If destination does not exist. Source is moved */
	else
	{
		printf("Moving %s into %s\n", source, destination);
		if (!move_file(source, destination, delete))
		{
			printf("Cannot move file\n");
			return false;
		}
		return true;
	}

	printf("Joining into %s\n", destination);
	file_append(source, destination);
	if (delete) unlink(source);

	return true;
}

/**
 * @brief Join two mz sources
 * 
 * @param source paht to source
 * @param destination  path to destination
 * @param skip_delete true to skip deletion
 */
/*void ldb_join_mz(char * table, char *source, char *destination, bool skip_delete, bool encrypted)
{
	char src_path[LDB_MAX_PATH] = "\0";
	char dst_path[LDB_MAX_PATH] = "\0";

	for (int i = 0; i < 65536; i++)
	{
		sprintf(src_path, "%s/%s/%04x.mz", source, table, i);
		sprintf(dst_path, "%s/%s/%04x.mz", destination, table, i);

		check_file_extension(src_path, encrypted);
		check_file_extension(dst_path, encrypted);

		ldb_bin_join(src_path, dst_path, false, skip_delete);
	}
	sprintf(src_path, "%s/%s", table, source);
	if (!skip_delete) rmdir(src_path);
}*/

/**
 * @brief  Join two snippets file
 * 
 * @param source path to source
 * @param destination path to destination
 * @param skip_delete true to skip deletion
 */
void ldb_join_snippets(char * table, char *source, char *destination, bool skip_delete)
{
	char src_path[LDB_MAX_PATH] = "\0";
	char dst_path[LDB_MAX_PATH] = "\0";

	for (int i = 0; i < 256; i++)
	{
		sprintf(src_path, "%s/%s/%02x.bin", source, table, i);
		sprintf(dst_path, "%s/%s/%02x.bin", destination, table, i);
		ldb_bin_join(src_path, dst_path, true, skip_delete);
	}
	sprintf(src_path, "%s/%s", source, table);
	if (!skip_delete) rmdir(src_path);
}

