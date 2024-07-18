#include "ldb_wrapper.h"
/*
 * @brief Queries a LDB table given a key
 * 
 * @param dbtable <database>/<tablename> to be queried
 * @param key key string to search
 */
T_RawRes * ldb_query_raw(char *dbtable, char *key) 
{
	/* Extract values from command */
	uint8_t *keybin = malloc(LDB_MAX_NODE_LN);
	char *rs = malloc(LDB_MAX_NODE_DATA_LN);
	if (ldb_valid_table(dbtable))
	{
		if (strlen(key) < 8) printf("E071 Key length cannot be less than 32 bits\n");
		else
		{	
			// Convert key to binary 
			ldb_hex_to_bin(key, strlen(key), keybin);
			int key_ln = (int) strlen(key) / 2;
	
			// Assembly ldb table structure 
			struct ldb_table ldbtable = ldb_read_cfg(dbtable);

			// Set hex dump width as fixed record length (default= 16) 
			int width = ldbtable.rec_ln;
			if (!width) width = 16;

			// Verify that provided key matches table key_ln (or main LDB_KEY_LEN) 
			if ((key_ln != ldbtable.key_ln) && (key_ln != LDB_KEY_LN))
				printf("E073 Provided key length is invalid\n");
			
			else
			{
			
			  	T_RawRes *results =malloc(sizeof( T_RawRes));
			 	results->data=malloc(10*LDB_MAX_NODE_DATA_LN);
	   			results->size=0;
				ldb_fetch_recordset(NULL, ldbtable, keybin, false, ldb_dump_row, results);

				free(dbtable);
				free(keybin);
				free(rs);
				return results;
			}
		}
	} 
	free(dbtable);
	free(keybin);
	free(rs);
	return NULL;
}



bool ldb_dump_row(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr) {
   
	T_RawRes *r=ptr;
	memcpy(&r->data[r->size],&size,4);
	memcpy(&r->data[r->size+4],data,size);
	r->size+=size+4;
	return false;
}
