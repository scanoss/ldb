#ifndef __COLLATE_H
#define __COLLATE_H
#include "../ldb.h"
struct ldb_collate_data;
typedef bool (*collate_handler)(struct ldb_collate_data *collate, uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size);


typedef struct tuple_t
{
	uint8_t key[MD5_LEN];
	int keys;
	char * data;
} tuple_t;

typedef struct job_delete_tuples_t
{
	tuple_t ** tuples;
	int tuples_number;
	int key_ln;
	int keys_number;
	collate_handler handler;
} job_delete_tuples_t;

struct ldb_collate_data
{
	void *data; 
	void *tmp_data;
	long data_ptr;
	int table_key_ln;
	int table_rec_ln;
	int max_rec_ln;
	long  rec_width;
	long rec_count;
	FILE *out_sector;
	struct ldb_table in_table;
	struct ldb_table out_table;
	uint8_t last_key[LDB_KEY_LN];
	time_t last_report;
	bool merge;
	long del_count;
	long key_rec_count;
	job_delete_tuples_t * del_tuples;
	collate_handler handler;
};
bool ldb_collate_init(struct ldb_collate_data * collate, struct ldb_table table, struct ldb_table out_table, int max_rec_ln, bool merge, uint8_t sector);
void ldb_collate_sector(struct ldb_collate_data *collate, ldb_sector_t * sector);
int ldb_collate_load_tuples_to_delete(job_delete_tuples_t* job, char * buffer, char * d, struct ldb_table table);
void ldb_collate(struct ldb_table table, struct ldb_table out_table, int max_rec_ln, bool merge, int p_sector, collate_handler handler);
void ldb_collate_delete(struct ldb_table table, struct ldb_table out_table, job_delete_tuples_t * delete, collate_handler handler);

#endif