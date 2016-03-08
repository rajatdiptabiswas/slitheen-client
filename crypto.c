#include <openssl/evp.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include "crypto.h"

/* PRF using sha384, as defined in RFC 5246 */
int PRF(uint8_t *secret, int32_t secret_len,
		uint8_t *seed1, int32_t seed1_len,
		uint8_t *seed2, int32_t seed2_len,
		uint8_t *seed3, int32_t seed3_len,
		uint8_t *seed4, int32_t seed4_len,
		uint8_t *output, int32_t output_len){

	EVP_MD_CTX ctx, ctx_tmp, ctx_init;
	EVP_PKEY *mac_key;
	const EVP_MD *md = EVP_sha384();

	uint8_t A[EVP_MAX_MD_SIZE];
	size_t len, A_len;
	int chunk = EVP_MD_size(md);
	int remaining = output_len;

	uint8_t *out = output;

	EVP_MD_CTX_init(&ctx);
	EVP_MD_CTX_init(&ctx_tmp);
	EVP_MD_CTX_init(&ctx_init);
	EVP_MD_CTX_set_flags(&ctx_init, EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);

	mac_key = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, secret, secret_len);

	/* Calculate first A value */
	EVP_DigestSignInit(&ctx_init, NULL, md, NULL, mac_key);
	EVP_MD_CTX_copy_ex(&ctx, &ctx_init);
	if(seed1 != NULL && seed1_len > 0){
		EVP_DigestSignUpdate(&ctx, seed1, seed1_len);
	}
	if(seed2 != NULL && seed2_len > 0){
		EVP_DigestSignUpdate(&ctx, seed2, seed2_len);
	}
	if(seed3 != NULL && seed3_len > 0){
		EVP_DigestSignUpdate(&ctx, seed3, seed3_len);
	}
	if(seed4 != NULL && seed4_len > 0){
		EVP_DigestSignUpdate(&ctx, seed4, seed4_len);
	}
	EVP_DigestSignFinal(&ctx, A, &A_len);

	//iterate until desired length is achieved
	while(remaining > 0){
		/* Now compute SHA384(secret, A+seed) */
		EVP_MD_CTX_copy_ex(&ctx, &ctx_init);
		EVP_DigestSignUpdate(&ctx, A, A_len);
		EVP_MD_CTX_copy_ex(&ctx_tmp, &ctx);
		if(seed1 != NULL && seed1_len > 0){
			EVP_DigestSignUpdate(&ctx, seed1, seed1_len);
		}
		if(seed2 != NULL && seed2_len > 0){
			EVP_DigestSignUpdate(&ctx, seed2, seed2_len);
		}
		if(seed3 != NULL && seed3_len > 0){
			EVP_DigestSignUpdate(&ctx, seed3, seed3_len);
		}
		if(seed4 != NULL && seed4_len > 0){
			EVP_DigestSignUpdate(&ctx, seed4, seed4_len);
		}
		
		if(remaining > chunk){
			EVP_DigestSignFinal(&ctx, out, &len);
			out += len;
			remaining -= len;

			/* Next A value */
			EVP_DigestSignFinal(&ctx_tmp, A, &A_len);
		} else {
			EVP_DigestSignFinal(&ctx, A, &A_len);
			memcpy(out, A, remaining);
			remaining -= remaining;
		}
	}
	return 1;
}
