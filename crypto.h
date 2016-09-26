#ifndef _CRYPTO_H_
#define _CRYPTO_H_

#include <stdint.h>
#include <openssl/evp.h>

# define n2s(c,s)        ((s=(((unsigned int)(c[0]))<< 8)| \
							(((unsigned int)(c[1]))    )),c+=2)


int PRF(uint8_t *secret, int32_t secret_len,
		uint8_t *seed1, int32_t seed1_len,
		uint8_t *seed2, int32_t seed2_len,
		uint8_t *seed3, int32_t seed3_len,
		uint8_t *seed4, int32_t seed4_len,
		uint8_t *output, int32_t output_len);

int super_decrypt(uint8_t *data);
int generate_super_keys(uint8_t *secret);

typedef struct super_data_st {
	uint8_t *header_key;
	uint8_t *body_key;
	EVP_MD_CTX *body_mac_ctx;
} super_data;

#define PRE_MASTER_LEN 256

#endif /* _CRYPTO_H_ */
