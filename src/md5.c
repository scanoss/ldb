#include "ldb.h"
#include <gcrypt.h>

/* Adapter function for compatibility with openssl*/
void md5_string(const unsigned char *input, int len, unsigned char output[16]) 
{
    gcry_md_hd_t h;
    gcry_md_open(&h, GCRY_MD_MD5, GCRY_MD_FLAG_SECURE);  // initialize the hash context
    gcry_md_write(h, input, len);  // hash the input data
    const unsigned char *hash_result = gcry_md_read(h, GCRY_MD_MD5);  // get the result

    // Copy the hash result to the output array
    memcpy(output, hash_result, 16);

    // Free memory for the hash result
    gcry_md_close(h);
}

/**
 * @brief Returns the hexadecimal md5 sum for "path"
 * 
 * @param path string path
 * @return pointer to file md5 array
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gcrypt.h>

static void __attribute__((constructor)) init_libgcrypt(void)
{
    if (!gcry_check_version(GCRYPT_VERSION)) {
        fprintf(stderr, "Libgcrypt version mismatch\n");
        return;
    }
    
    gcry_control(GCRYCTL_DISABLE_SECMEM_WARN);
    gcry_control(GCRYCTL_INIT_SECMEM, 16384, 0);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
}

uint8_t * md5_file(char *path)
{ 
    uint8_t *c = calloc(1, gcry_md_get_algo_dlen(GCRY_MD_MD5)); // Allocate memory for MD5 hash
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Unable to open file for reading.\n");
        free(c);
        return NULL;
    }
    
    gcry_md_hd_t md5_hd;
    gcry_md_open(&md5_hd, GCRY_MD_MD5, GCRY_MD_FLAG_SECURE);
    uint8_t *buffer = malloc(BUFFER_SIZE);
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        gcry_md_write(md5_hd, buffer, bytes);
    }
    
    fclose(fp);
    free(buffer);
    
    const uint8_t *digest = gcry_md_read(md5_hd, GCRY_MD_MD5);
    memcpy(c, digest, gcry_md_get_algo_dlen(GCRY_MD_MD5));
    gcry_md_close(md5_hd);
    
    return c;
}