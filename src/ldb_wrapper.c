#include "ldb_wrapper.h"
/**
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

			// Verify that provided key matches table key_ln (or main LDB_KEY_LEN) 
			if ((key_ln != ldbtable.key_ln) && (key_ln != LDB_KEY_LN))
				printf("E073 Provided key length is invalid\n");
			
			else
			{
			  	T_RawRes *results =malloc(sizeof( T_RawRes));
			 	results->data=malloc(LDB_MAX_NODE_DATA_LN);
	   			results->size=0;
                results->capacity=LDB_MAX_NODE_DATA_LN;
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
/**
 * @brief Function handle to retrieve a recordset
 * @details Appends to T_RawRes output a record for a that key. As result size is unknown,
*           this function realocate memory in chunks.
 * @param key key of the record to be fetched
 * @param subkey  subkey of the record to be fetched
 * @param subkey_ln number of byes considering a subkey
 * @param data data fetched
 * @param size size of the fetched record
 * @param iteration record index 
 * @param ptr result pointer
 */


bool ldb_dump_row(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t size, int iteration, void *ptr) {
   
	T_RawRes *r=ptr;
    if (r->size + size > r->capacity) {
        size_t new_capacity = r->capacity + 	2*LDB_MAX_NODE_DATA_LN;
        while (r->size + size > new_capacity) {
            new_capacity += 2*LDB_MAX_NODE_DATA_LN;
        }
        uint8_t *new_data = realloc(r->data, new_capacity);
        if (!new_data) {
            perror("Failed to reallocate memory");
            exit(EXIT_FAILURE);
        }
        r->data = new_data;
        r->capacity = new_capacity;
    }

	memcpy(&r->data[r->size],&size,4);
	memcpy(&r->data[r->size+4],data,size);
	r->size+=size+4;

	return false;
}
