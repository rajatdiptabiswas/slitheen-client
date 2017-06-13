#include <string.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

#include "util.h"

/*
 * Base64 encodes input bytes and stores them in out
 */
int base64_encode(uint8_t *in, size_t len, char **out){

    //b64 encode slitheen ID
    BUF_MEM *buffer_ptr;
    BIO *bio, *b64;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, in, len);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);
    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(bio);
    *out = (*buffer_ptr).data;
    //*out[(*buffer_ptr).length] = '\0';

    return 1;
}

//malloc macro that exits on error
void *emalloc(size_t size){
    void *ptr = malloc(size);
    if (ptr == NULL){
        fprintf(stderr, "Memory failure. Exiting...\n");
	exit(1);
    }

    return ptr;
}

//calloc macro that exits on error
void *ecalloc(size_t nmemb, size_t size){
    void *ptr = calloc(nmemb, size);
    if(ptr == NULL){
        fprintf(stderr, "Memory failure. Exiting...\n");
        exit(1);
    }

    return ptr;
}
