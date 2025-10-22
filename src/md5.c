#include "ldb.h"
#include <gcrypt.h>
#include <pthread.h>

/* Adapter function for compatibility with openssl*/
void md5_string(const unsigned char *input, int len, unsigned char output[16])
{
    gcry_md_hd_t h;
    /* Don't use GCRY_MD_FLAG_SECURE to avoid secure memory issues */
    gcry_md_open(&h, GCRY_MD_MD5, 0);  // initialize the hash context without secure flag
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

static pthread_once_t gcrypt_init_once = PTHREAD_ONCE_INIT;
static int gcrypt_initialized = 0;
static pthread_mutex_t gcrypt_mutex = PTHREAD_MUTEX_INITIALIZER;

static void init_libgcrypt_internal(void)
{
    pthread_mutex_lock(&gcrypt_mutex);

    /* Double-check if already initialized */
    if (gcrypt_initialized) {
        pthread_mutex_unlock(&gcrypt_mutex);
        return;
    }

    /* Check version */
    if (!gcry_check_version(GCRYPT_VERSION)) {
        fprintf(stderr, "Libgcrypt version mismatch\n");
        pthread_mutex_unlock(&gcrypt_mutex);
        return;
    }

    /* Check if already initialized at library level */
    if (gcry_control(GCRYCTL_INITIALIZATION_FINISHED_P)) {
        gcrypt_initialized = 1;
        pthread_mutex_unlock(&gcrypt_mutex);
        return;
    }

    /* Disable secure memory warnings completely */
    gcry_control(GCRYCTL_DISABLE_SECMEM_WARN);

    /* Skip secure memory initialization - use standard malloc instead */
    /* This avoids the "Oops" message when forking processes */
    gcry_control(GCRYCTL_DISABLE_SECMEM, 0);

    /* Mark as initialized */
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

    gcrypt_initialized = 1;
    pthread_mutex_unlock(&gcrypt_mutex);
}

static void __attribute__((constructor)) init_libgcrypt(void)
{
    pthread_once(&gcrypt_init_once, init_libgcrypt_internal);
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
    gcry_md_open(&md5_hd, GCRY_MD_MD5, 0);  // Don't use secure flag to avoid issues
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