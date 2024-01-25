#ifndef __DECODE_H
#define __DECODE_H
#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdbool.h>
#define DECODE_BASE64 8
extern int (*decode) (int op, unsigned char *key, unsigned char *nonce,
		        const char *buffer_in, int buffer_in_len, unsigned char *buffer_out);
extern void * lib_handle;
bool ldb_decoder_lib_load(void);
void ldb_decoder_lib_close(void);
#endif