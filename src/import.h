#ifndef __IMPORT_H
#define __IMPORT_H
#include <stdbool.h>
#include "ldb.h"

typedef union import_params {
	struct __attribute__((__packed__)) params
	{
		int delete_after_import;
    	int keys_number;
		int overwrite;
		int skip_sort;
		int version_validation;
		int verbose;
		int is_mz_table;
    	int binary_mode;
		int is_wfp_table;
    	int csv_fields;
    	int skip_fields_check;
		int collate;
		int collate_max_rec;
		int collate_max_ram_percent;
	} params;
	int params_arr[sizeof(struct params)/sizeof(int)];
} import_params_t;

typedef struct ldb_importation_config_t
{
    char path[LDB_MAX_PATH];
    char tmp_path[LDB_MAX_PATH];
    char dbname[LDB_MAX_NAME];
    char table[LDB_MAX_NAME];
    char csv_path[LDB_MAX_PATH];
	import_params_t opt;
} ldb_importation_config_t;


bool ldb_importation_config_parse(ldb_importation_config_t * conf, char * line);
bool ldb_create_db_config_default(char * dbname);
int ldb_import(ldb_importation_config_t * job);
uint64_t ldb_file_size(char *path);

#endif