/** SOCKSv5 proxy that listens on port 1080.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include<openssl/buffer.h>

#define SLITHEEN_ID_LEN 10

#define NEW

int proxy_data(int sockfd, uint8_t stream_id, int32_t pipefd);
void *demultiplex_data();

struct __attribute__ ((__packed__)) slitheen_hdr{
	uint8_t stream_id;
	uint16_t len;
	uint16_t garbage_len;
};

#define SLITHEEN_HEADER_LEN 5

struct __attribute__ ((__packed__)) slitheen_up_hdr{
	uint8_t stream_id;
	uint16_t len;
};

typedef struct connection_st{
	int32_t pipe_fd;
	uint8_t stream_id;
	struct connection_st *next;
} connection;

typedef struct connection_table_st{
	connection *first;
} connection_table;

static connection_table *connections;

int main(void){
	int listen_socket;
	
	struct sockaddr_in address;
	struct sockaddr_in remote_addr;
	socklen_t addr_size;

	mkfifo("OUS_out", 0666);

	//randomly generate slitheen id
	uint8_t slitheen_id[SLITHEEN_ID_LEN];
	RAND_bytes(slitheen_id, SLITHEEN_ID_LEN);
	printf("Randomly generated slitheen id: ");
	int i;
	for(i=0; i< SLITHEEN_ID_LEN; i++){
		printf("%02x ", slitheen_id[i]);
	}
	printf("\n");

	//b64 encode slitheen ID
	const char *encoded_bytes;
	BUF_MEM *buffer_ptr;
	BIO *bio, *b64;
	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new(BIO_s_mem());
	bio = BIO_push(b64, bio);

	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	BIO_write(bio, slitheen_id, SLITHEEN_ID_LEN);
	BIO_flush(bio);
	BIO_get_mem_ptr(bio, &buffer_ptr);
	BIO_set_close(bio, BIO_NOCLOSE);
	BIO_free_all(bio);
	encoded_bytes = (*buffer_ptr).data;

	//give encoded slitheen ID to ous
	struct sockaddr_in ous_addr;
	ous_addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &(ous_addr.sin_addr));
	ous_addr.sin_port = htons(8888);

	int32_t ous_in = socket(AF_INET, SOCK_STREAM, 0);
	if(ous_in < 0){
		printf("Failed to make ous_in socket\n");
		return 1;
	}

	int32_t error = connect(ous_in, (struct sockaddr *) &ous_addr, sizeof (struct sockaddr));
	if(error < 0){
		printf("Error connecting\n");
		return 1;
	}
	uint8_t *message = calloc(1, BUFSIZ);
	sprintf(message, "POST / HTTP/1.1\r\nContent-Length: %d\r\n\r\n%s ", strlen(encoded_bytes), encoded_bytes);
	int32_t bytes_sent = send(ous_in, message, strlen(message), 0);
	printf("Wrote %d bytes to OUS_in: %s\n", bytes_sent, message);
	free(message);

	/* Spawn process to listen for incoming data from OUS 
	int32_t demux_pipe[2];
	if(pipe(demux_pipe) < 0){
		printf("Failed to create pipe for new thread\n");
		return 1;
	}*/
	connections = calloc(1, sizeof(connection_table));
	connections->first = NULL;
	
	pthread_t *demux_thread = calloc(1, sizeof(pthread_t));
	pthread_create(demux_thread, NULL, demultiplex_data, NULL);

	if (!(listen_socket = socket(AF_INET, SOCK_STREAM, 0))){
		printf("Error creating socket\n");
		fflush(stdout);
		return 1;
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(1080);

	int enable = 1;
	if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) <0 ){
		printf("Error setting sockopt\n");
		return 1;
	}

	if(bind(listen_socket, (struct sockaddr *) &address, sizeof(address))){
		printf("Error binding socket\n");
		fflush(stdout);
		return 1;
	}

	if(listen(listen_socket, 10) < 0){
		printf("Error listening\n");
		fflush(stdout);
		close(listen_socket);
		exit(1);
	}
	uint8_t last_id = 1;

	printf("Ready for listening\n");

	for(;;){
		addr_size = sizeof(remote_addr);
		int new_socket;
		new_socket = accept(listen_socket, (struct sockaddr *) &remote_addr,
						&addr_size);
		if(new_socket < 0){
			perror("accept");
			exit(1);
		}
		printf("New connection\n");

		//assign a new stream_id and create a pipe for the session
		connection *new_conn = calloc(1, sizeof(connection));
		new_conn->stream_id = last_id++;
		
		int32_t pipefd[2];
		if(pipe(pipefd) < 0){
			printf("Failed to create pipe\n");
			continue;
		}

		new_conn->pipe_fd = pipefd[1];
		new_conn->next = NULL;
		
		if(connections->first == NULL){
			connections->first = new_conn;
			printf("Added first connection with id: %d\n", new_conn->stream_id);
			printf("Connection table (%p) has entry %p\n", connections, connections->first);
			fflush(stdout);
		} else {
			connection *last = connections->first;
			printf("New incoming connection\n");
			fflush(stdout);
			while(last->next != NULL){
				last = last->next;
			}
			last->next = new_conn;
			printf("Added connection with id: %d at %p\n", new_conn->stream_id, last->next);
			fflush(stdout);
		}

		int pid = fork();
		if(pid == 0){ //child

			close(listen_socket);
			printf("demux reads from pipe fd %d", pipefd[1]);
			fflush(stdout);
			proxy_data(new_socket, new_conn->stream_id, pipefd[0]);
			exit(0);
		}

		close(new_socket);
		
	}

	return 0;
}

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

//continuously read from the socket and look for a CONNECT message
int proxy_data(int sockfd, uint8_t stream_id, int32_t ous_out){
	uint8_t *buffer = calloc(1, BUFSIZ);
	uint8_t *response = calloc(1, BUFSIZ);
	printf("ous out pipe fd: %d\n", ous_out);
		fflush(stdout);
	
	int bytes_read = recv(sockfd, buffer, BUFSIZ-1, 0);
	if (bytes_read < 0){
		printf("Error reading from socket (fd = %d)\n", sockfd);
		fflush(stdout);
		goto err;
	}

	printf("Received %d bytes (id %d):\n", bytes_read, stream_id);
	int i;
	for(i=0; i< bytes_read; i++){
		printf("%02x ", buffer[i]);
	}
	printf("\n");
		fflush(stdout);

	//Respond to methods negotiation
	struct socks_method_req *clnt_meth = (struct socks_method_req *) buffer;
	uint8_t *p = buffer + 2;

	if(clnt_meth->version != 0x05){
		printf("Client supplied invalid version: %02x\n", clnt_meth->version);
		fflush(stdout);
	}

	int responded = 0;
	int bytes_sent;
	for(i=0; i< clnt_meth->num_methods; i++){
		if(p[0] == 0x00){//send response with METH= 0x00
			response[0] = 0x05;
			response[1] = 0x00;
			send(sockfd, response, 2, 0);
			responded = 1;
		}
		p++;
	}
	if(!responded){//respond with METH= 0xFF
		response[0] = 0x05;
		response[1] = 0xFF;
		send(sockfd, response, 2, 0);
		goto err;
	}

	//Now wait for a connect request
	bytes_read = recv(sockfd, buffer, BUFSIZ-1, 0);
	if (bytes_read < 0){
		printf("Error reading from socket\n");
		fflush(stdout);
		goto err;
	}

	printf("Received %d bytes (id %d):\n", bytes_read, stream_id);
	for(i=0; i< bytes_read; i++){
		printf("%02x ", buffer[i]);
	}
	printf("\n");
		fflush(stdout);

	//Now respond
	response[0] = 0x05;
	response[1] = 0x00;
	response[2] = 0x00;
	response[3] = 0x01;

	*((uint32_t *) (response + 4)) = 0;
	*((uint16_t *) (response + 8)) = 0;

	send(sockfd, response, 10, 0);

	//wait for first upstream bytes
	bytes_read += recv(sockfd, buffer+bytes_read, BUFSIZ-bytes_read-3, 0);
	if (bytes_read < 0){
		printf("Error reading from socket\n");
		fflush(stdout);
		goto err;
	}

	printf("Received %d bytes (id %d):\n", bytes_read, stream_id);
	for(i=0; i< bytes_read; i++){
		printf("%02x ", buffer[i]);
	}
	printf("\n");
	fflush(stdout);

	//pre-pend stream_id and length
	memmove(buffer+3, buffer, bytes_read+1);

	struct slitheen_up_hdr *up_hdr = (struct slitheen_up_hdr *) buffer;
	up_hdr->stream_id = stream_id;
	up_hdr->len = htons(bytes_read);

	bytes_read+= 3;

	//encode bytes for safe transport (b64)
	const char *encoded_bytes;
	BUF_MEM *buffer_ptr;
	BIO *bio, *b64;
	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new(BIO_s_mem());
	bio = BIO_push(b64, bio);

	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	BIO_write(bio, buffer, bytes_read);
	BIO_flush(bio);
	BIO_get_mem_ptr(bio, &buffer_ptr);
	BIO_set_close(bio, BIO_NOCLOSE);
	BIO_free_all(bio);
	encoded_bytes = (*buffer_ptr).data;

#ifdef NEW
	struct sockaddr_in ous_addr;
	ous_addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &(ous_addr.sin_addr));
	ous_addr.sin_port = htons(8888);

	int32_t ous_in = socket(AF_INET, SOCK_STREAM, 0);
	if(ous_in < 0){
		printf("Failed to make ous_in socket\n");
		return 1;
	}

	int32_t error = connect(ous_in, (struct sockaddr *) &ous_addr, sizeof (struct sockaddr));
	if(error < 0){
		printf("Error connecting\n");
		return 1;
	}
#endif

	//send connect request to OUS
#ifdef OLD
	int ous_in = open("OUS_in", O_CREAT | O_WRONLY, 0666);
	if(ous_in < 0){
		printf("Error opening file OUS_in\n");
		fflush(stdout);
		goto err;
	}

	lseek(ous_in, 0, SEEK_END);
#endif

#ifdef NEW
	uint8_t *message = calloc(1, BUFSIZ);
	sprintf(message, "POST / HTTP/1.1\r\nContent-Length: %d\r\n\r\n%s ", strlen(encoded_bytes)+1, encoded_bytes);
	bytes_sent = send(ous_in, message, strlen(message), 0);
	printf("Wrote %d bytes to OUS_in: %s\n", bytes_sent, message);
#endif

#ifdef OLD
	bytes_sent = write(ous_in, encoded_bytes, strlen(encoded_bytes));
	bytes_sent += write(ous_in, " ", 1);
#endif
	if(bytes_sent < 0){
		printf("Error writing to websocket\n");
		fflush(stdout);
		goto err;
	} else {
		close(ous_in);
	}

	p = buffer+sizeof(struct slitheen_up_hdr);
	for(i=0; i< bytes_read; i++){
		printf("%02x ", p[i]);
	}
	printf("\n");
		fflush(stdout);
	struct socks_req *clnt_req = (struct socks_req *) p;
	p += 4;

	//see if it's a connect request
	if(clnt_req->cmd != 0x01){
		printf("Error: issued a non-connect command\n");
		fflush(stdout);
		goto err;
	}
	printf("Received a connect request from stream id %d\n", stream_id);
		fflush(stdout);

	//now select on pipe (for downstream data) and the socket (for upstream data)
	for(;;){

		fd_set readfds;
		fd_set writefds;

		int32_t nfds = (sockfd > ous_out) ? sockfd +1 : ous_out + 1;
		//if(sockfd > ous_out){
		//	nfds = (sockfd > ous_in) ? sockfd +1 : ous_in + 1;
		//} else {
		//	nfds = (ous_out > ous_in) ? ous_out +1 : ous_in + 1;
		//}

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		FD_SET(sockfd, &readfds);
		FD_SET(ous_out, &readfds);
		FD_SET(sockfd, &writefds);
		//FD_SET(ous_in, &writefds);

		if(select(nfds, &readfds, &writefds, NULL, NULL) <0){
			printf("Select error\n");
		fflush(stdout);
			continue;
		}

		if(FD_ISSET(sockfd, &readfds)){// && FD_ISSET(ous_in, &writefds)){

			bytes_read = recv(sockfd, buffer, BUFSIZ-1, 0);
			if (bytes_read < 0){
				printf("Error reading from socket (in for loop)\n");
		fflush(stdout);
				goto err;
			}
			if(bytes_read == 0){
				//socket is closed
				printf("Closing connection for stream %d sockfd.\n", stream_id);
				fflush(stdout);

				//Send close message to slitheen proxy
				up_hdr = (struct slitheen_up_hdr *) buffer;
				up_hdr->stream_id = stream_id;
				up_hdr->len = 0;
				bio = BIO_new(BIO_s_mem());
				b64 = BIO_new(BIO_f_base64());
				bio = BIO_push(b64, bio);

				BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
				BIO_write(bio, buffer, 20);
				BIO_flush(bio);
				BIO_get_mem_ptr(bio, &buffer_ptr);
				BIO_set_close(bio, BIO_NOCLOSE);
				BIO_free_all(bio);
				encoded_bytes = (*buffer_ptr).data;
				ous_in = socket(AF_INET, SOCK_STREAM, 0);
				if(ous_in < 0){
					printf("Failed to make ous_in socket\n");
				fflush(stdout);
					goto err;
				}

				error = connect(ous_in, (struct sockaddr *) &ous_addr, sizeof (struct sockaddr));
				if(error < 0){
					printf("Error connecting\n");
				fflush(stdout);
					goto err;
				}

				sprintf(message, "POST / HTTP/1.1\r\nContent-Length: %d\r\n\r\n%s ", strlen(encoded_bytes)+1, encoded_bytes);
				bytes_sent = send(ous_in, message, strlen(message), 0);
				close(ous_in);

				goto err;
				
			}

			if(bytes_read > 0){

				printf("Received %d data bytes from sockfd (id %d):\n", bytes_read, stream_id);
				for(i=0; i< bytes_read; i++){
					printf("%02x ", buffer[i]);
				}
				printf("\n");
				printf("%s\n", buffer);
		fflush(stdout);
				memmove(buffer+sizeof(struct slitheen_up_hdr), buffer, bytes_read);

				up_hdr = (struct slitheen_up_hdr *) buffer;
				up_hdr->stream_id = stream_id;
				up_hdr->len = htons(bytes_read);

				bytes_read+= 3;

				bio = BIO_new(BIO_s_mem());
				b64 = BIO_new(BIO_f_base64());
				bio = BIO_push(b64, bio);

				printf("HERE\n");
				fflush(stdout);
				BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
				BIO_write(bio, buffer, bytes_read);
				BIO_flush(bio);
				BIO_get_mem_ptr(bio, &buffer_ptr);
				BIO_set_close(bio, BIO_NOCLOSE);
				BIO_free_all(bio);
				encoded_bytes = (*buffer_ptr).data;
				
#ifdef OLD
				int ous_in = open("OUS_in", O_CREAT | O_WRONLY, 0666);
				if(ous_in < 0){
					printf("Error opening file OUS_in\n");
					fflush(stdout);
					goto err;
				}

				lseek(ous_in, 0, SEEK_END);
#endif

#ifdef NEW
				ous_in = socket(AF_INET, SOCK_STREAM, 0);
				if(ous_in < 0){
					printf("Failed to make ous_in socket\n");
					return 1;
				}

				error = connect(ous_in, (struct sockaddr *) &ous_addr, sizeof (struct sockaddr));
				if(error < 0){
					printf("Error connecting\n");
					return 1;
				}

				sprintf(message, "POST / HTTP/1.1\r\nContent-Length: %d\r\n\r\n%s ", strlen(encoded_bytes)+1, encoded_bytes);
				bytes_sent = send(ous_in, message, strlen(message), 0);
				printf("Sent to OUS (%d bytes):%s\n",bytes_sent, message);
				close(ous_in);

#endif

#ifdef OLD
				bytes_sent = write(ous_in, encoded_bytes, strlen(encoded_bytes));
				bytes_sent += write(ous_in, " ", 1);
				printf("Sent to OUS (%d bytes):%s\n",bytes_sent, encoded_bytes);
				close(ous_in);
#endif
			}
		} else if(FD_ISSET(ous_out, &readfds) && FD_ISSET(sockfd, &writefds)){

			bytes_read = read(ous_out, buffer, BUFSIZ-1);
			if (bytes_read <= 0){
				printf("Error reading from ous_out (in for loop)\n");
		fflush(stdout);
				goto err;
			}

			if(bytes_read > 0){

				printf("Stream id %d received %d bytes from ous_out:\n", stream_id, bytes_read);
				for(i=0; i< bytes_read; i++){
					printf("%02x ", buffer[i]);
				}
				printf("\n");
				printf("%s\n", buffer);
		fflush(stdout);

				bytes_sent = send(sockfd, buffer, bytes_read, 0);
				if(bytes_sent <= 0){
					printf("Error sending bytes to browser for stream id %d\n", stream_id);
				}
				
				printf("Sent to browser (%d bytes from stream id %d):\n", bytes_sent, stream_id);
				for(i=0; i< bytes_sent; i++){
					printf("%02x ", buffer[i]);
				}
				printf("\n");
		fflush(stdout);

			}
		}
	}


err:
		//should also remove stream from table
	close(sockfd);
	free(buffer);
	free(response);
	exit(0);
}

void *demultiplex_data(){

	int32_t buffer_len = BUFSIZ;
	uint8_t *buffer = calloc(1, buffer_len);
	uint8_t *p;

	printf("Opening OUS_out\n");
	int32_t ous_fd = open("OUS_out", O_RDONLY);
	printf("Opened.\n");
	uint8_t *overflow;
	uint32_t overflow_len = 0;

	for(;;){
		int32_t bytes_read = read(ous_fd, buffer, buffer_len-overflow_len);
		
		if(bytes_read > 0){
			int32_t bytes_remaining = bytes_read;

			if(overflow_len > 0){
				//process first part of slitheen info
				printf("Completeing previously read header\n");
				memmove(buffer+overflow_len, buffer, bytes_read);
				memcpy(buffer, overflow, overflow_len);
				bytes_remaining += overflow_len;
				free(overflow);
				overflow_len = 0;
			}

			p = buffer;
			while(bytes_remaining > 0){
				if(bytes_remaining < SLITHEEN_HEADER_LEN){
					printf("Partial header: ");
					int i;
					for(i = 0; i< bytes_remaining; i++){
						printf("%02x ", p[i]);
					}
					printf("\n");
				}

				struct slitheen_hdr *sl_hdr = (struct slitheen_hdr *) p;
				//first see if sl_hdr corresponds to a valid stream. If not, ignore rest of read bytes
#ifdef DEBUG
				printf("Slitheen header:\n");
				int i;
				for(i = 0; i< SLITHEEN_HEADER_LEN; i++){
					printf("%02x ", p[i]);
				}
				printf("\n");
#endif

				p += sizeof(struct slitheen_hdr);

				if(sl_hdr->stream_id == 0){
#ifdef DEBUG
					printf("Garbage bytes\n");
#endif
					p += ntohs(sl_hdr->len);
					bytes_remaining -= sizeof(struct slitheen_hdr) + ntohs(sl_hdr->len);
					continue;
				}

				int32_t pipe_fd =-1;
				if(connections->first == NULL){
					printf("There are no connections\n");
				} else {
					connection *last = connections->first;
					if (last->stream_id == sl_hdr->stream_id){
						printf("Found stream id %d!\n", sl_hdr->stream_id);
						pipe_fd = last->pipe_fd;
						printf("Pipe fd: %d\n", pipe_fd);
					}
					while(last->next != NULL){
						last = last->next;
						if (last->stream_id == sl_hdr->stream_id){
							printf("Found stream id %d!\n", sl_hdr->stream_id);
							pipe_fd = last->pipe_fd;
							printf("Pipe fd: %d\n", pipe_fd);
						}
					}
				}
				
				if(pipe_fd == -1){
					printf("No stream id exists. Possibly invalid header\n");
					break;
				}
				
				if(ntohs(sl_hdr->len)+ sizeof(struct slitheen_hdr) > bytes_remaining){
					overflow = calloc(1, bytes_remaining);
					memcpy(overflow, p, bytes_remaining);
					overflow_len = bytes_remaining;
					bytes_remaining = 0;
					break;
				}

				if(sl_hdr->garbage_len == 0){
					printf("Received information for stream id: %d of length: %u\n", sl_hdr->stream_id, ntohs(sl_hdr->len));

					int32_t bytes_sent = write(pipe_fd, p, ntohs(sl_hdr->len));
					if(bytes_sent <= 0){
						printf("Error reading to pipe for stream id %d\n", sl_hdr->stream_id);
					}
				}

				p += ntohs(sl_hdr->len);
				bytes_remaining -= sizeof(struct slitheen_hdr) + ntohs(sl_hdr->len);
			}

		} else {
			printf("Error: read %d bytes from OUS_out\n", bytes_read);
			printf("Opening OUS_out\n");
			close(ous_fd);
			ous_fd = open("OUS_out", O_RDONLY);
			printf("Opened.\n");
		}
		
	}

	close(ous_fd);

}
