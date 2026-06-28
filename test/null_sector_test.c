/* Regression test for ldb_fetch_recordset's documented NULL-sector contract.
 *
 * The public API states the `sector` argument is optional ("If NULL the function
 * will use the table struct and key to open the ldb"). The CLI always passes a
 * stack sector, so this path is only reachable from C; this test exercises it.
 *
 * Usage: null_sector_test db/table hexkey
 * Exits 0 if at least one record is returned (and, crucially, no segfault).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ldb.h>

static bool count_handler(struct ldb_table *table, uint8_t *key, uint8_t *subkey,
                          uint8_t *data, uint32_t data_len, int record_number, void *ptr)
{
	(void) table; (void) key; (void) subkey; (void) data; (void) data_len; (void) record_number;
	(*(int *)ptr)++;
	return false; /* keep iterating */
}

int main(int argc, char **argv)
{
	if (argc != 3) { fprintf(stderr, "usage: %s db/table hexkey\n", argv[0]); return 2; }

	struct ldb_table table = ldb_read_cfg(argv[1]);
	uint8_t key[64] = {0};
	ldb_hex_to_bin(argv[2], strlen(argv[2]), key);

	int count = 0;
	/* The point of the test: pass NULL as the sector (documented usage). */
	uint32_t records = ldb_fetch_recordset(NULL, table, key, false, count_handler, &count);

	printf("%s key %s -> records=%u handler_calls=%d\n", argv[1], argv[2], records, count);
	return (records > 0) ? 0 : 1;
}
