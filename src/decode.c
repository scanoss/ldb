#include "decode.h"
#include "logger.h"
void * lib_handle = NULL;

int (*decode) (int op, unsigned char *key, unsigned char *nonce,
		        const char *buffer_in, int buffer_in_len, unsigned char *buffer_out) = NULL;

bool ldb_decoder_lib_load(void)
{
	if (lib_handle != NULL)
		return true;
	/*set decode funtion pointer to NULL*/
	decode = NULL;
	lib_handle = dlopen("libscanoss_encoder.so", RTLD_NOW);
	char * err;
    if (lib_handle) 
	{
		log_info("Lib scanoss_encoder present\n");
		decode = dlsym(lib_handle, "scanoss_decode");
		if ((err = dlerror())) 
		{
			log_info("%s\n", err);
			return false;
		}
		return true;
     }
	 
	 return false;
}

void ldb_decoder_lib_close(void)
{
	dlclose(lib_handle);
}
