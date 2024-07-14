#include "./ldb/definitions.h"

int hash_len = MD5_LEN;

void hash_set_size(int size)
{
    hash_len = size;
}