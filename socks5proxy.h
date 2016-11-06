#ifndef _SOCKS5PROXY_H_
#define _SOCKS5PROXY_H_

#include <stdint.h>

#define SLITHEEN_ID_LEN 28
#define SLITHEEN_SUPER_SECRET_SIZE 16
#define SLITHEEN_SUPER_CONST "SLITHEEN_SUPER_ENCRYPT"
#define SLITHEEN_SUPER_CONST_SIZE 22

int proxy_data(int sockfd, uint16_t stream_id, int32_t pipefd);
void *demultiplex_data();

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

typedef struct data_block_st {
	uint64_t count;
	uint8_t *data;
        uint16_t len;
        int32_t pipe_fd;
	struct data_block_st *next;
} data_block;

struct socks_method_req {
	uint8_t version;
	uint8_t num_methods;
};

struct socks_req {
	uint8_t version;
	uint8_t cmd;
	uint8_t rsvd;
	uint8_t addr_type;
};

#endif /* _SOCKS5PROXY_H_ */
