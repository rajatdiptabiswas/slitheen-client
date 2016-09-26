#ifndef _SOCKS5PROXY_H_
#define _SOCKS5PROXY_H_

#include <openssl/evp.h>
#include <stdint.h>

#define SLITHEEN_ID_LEN 28
#define SLITHEEN_SUPER_SECRET_SIZE 28
#define SLITHEEN_SUPER_CONST "SLITHEEN_SUPER_ENCRYPT"
#define SLITHEEN_SUPER_CONST_SIZE 22

int proxy_data(int sockfd, uint16_t stream_id, int32_t pipefd);
void *demultiplex_data();
int super_decrypt(uint8_t *data);
int generate_super_keys(uint8_t *secret);

struct __attribute__ ((__packed__)) slitheen_hdr {
	uint64_t counter;
	uint16_t stream_id;
	uint16_t len;
	uint16_t garbage;
	uint16_t zeros;
};

#define SLITHEEN_HEADER_LEN 16

struct __attribute__ ((__packed__)) slitheen_up_hdr{
	uint16_t stream_id;
	uint16_t len;
};

typedef struct connection_st{
	int32_t pipe_fd;
	uint16_t stream_id;
	struct connection_st *next;
} connection;

typedef struct connection_table_st{
	connection *first;
} connection_table;

static connection_table *connections;

typedef struct super_data_st {
	//EVP_CIPHER_CTX *header_ctx;
	//EVP_CIPHER_CTX *body_ctx;
	uint8_t *header_key;
	uint8_t *body_key;
	EVP_MD_CTX *body_mac_ctx;
} super_data;

static super_data *super;

typedef struct data_block_st {
	uint64_t count;
	uint8_t *data;
	struct data_block_st *next;
} data_block;

#endif /* _SOCKS5PROXY_H_ */
