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
#include <netinet/in.h>
#include <netdb.h>

int proxy_data(int sockfd);

int main(void){
	int listen_socket;
	
	struct sockaddr_in address;
	struct sockaddr_in remote_addr;
	socklen_t addr_size;
	if (!(listen_socket = socket(AF_INET, SOCK_STREAM, 0))){
		printf("Error creating socket\n");
		return 1;
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(1080);

	if(bind(listen_socket, (struct sockaddr *) &address, sizeof(address))){
		printf("Error binding socket\n");
		return 1;
	}

	if(listen(listen_socket, 10) < 0){
		printf("Error listening\n");
		close(listen_socket);
		exit(1);
	}

	for(;;){
		addr_size = sizeof(remote_addr);
		int new_socket;
		new_socket = accept(listen_socket, (struct sockaddr *) &remote_addr,
						&addr_size);
		if(new_socket < 0){
			perror("accept");
			exit(1);
		}

		int pid = fork();
		if(pid > 0){ //child

			close(listen_socket);
			proxy_data(new_socket);
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
int proxy_data(int sockfd){
	uint8_t *buffer = calloc(1, BUFSIZ);
	uint8_t *response = calloc(1, BUFSIZ);
	
	int bytes_read = recv(sockfd, buffer, BUFSIZ-1, 0);
	if (bytes_read < 0){
		printf("Error reading from socket\n");
		goto err;
	}

	printf("Received %d bytes:\n", bytes_read);
	for(int i=0; i< bytes_read; i++){
		printf("%02x ", buffer[i]);
	}
	printf("\n");

	//Respond to methods negotiation
	struct socks_method_req *clnt_meth = (struct socks_method_req *) buffer;
	uint8_t *p = buffer + 2;

	if(clnt_meth->version != 0x05){
		printf("Client supplied invalid version: %02x\n", clnt_meth->version);
	}

	int responded = 0;
	int bytes_sent;
	for(int i=0; i< clnt_meth->num_methods; i++){
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
		goto err;
	}

	printf("Received %d bytes:\n", bytes_read);
	for(int i=0; i< bytes_read; i++){
		printf("%02x ", buffer[i]);
	}
	printf("\n");


	struct socks_req *clnt_req = (struct socks_req *) buffer;
	p = buffer+4;

	//see if it's a connect request
	if(clnt_req->cmd != 0x01){
		printf("Error: issued a non-connect command\n");
		goto err;
	}

	//from this point on, this code will live on slitheen relay

    struct sockaddr_in dest;
	dest.sin_family = AF_INET;
	uint8_t domain_len;

	switch(clnt_req->addr_type){
	case 0x01:
		//IPv4
		dest.sin_addr.s_addr = *((uint32_t*) p);
		printf("destination addr: %d\n", ntohl(dest.sin_addr.s_addr));
		p += 4;
		break;
		
	case 0x03:
		//domain name
		domain_len = p[0];
		p++;
		uint8_t *domain_name = calloc(1, domain_len+1);
		memcpy(domain_name, p, domain_len);
		domain_name[domain_len] = '\0';
		struct hostent *host;
		host = gethostbyname((const char *) domain_name);
		dest.sin_addr = *((struct in_addr *) host->h_addr);
		printf("destination addr: %d\n", ntohl(dest.sin_addr.s_addr));

		p += domain_len;
		free(domain_name);
		break;
	case 0x04:
		//IPv6
		goto err;//TODO: fix this
		break;
	}

	//now set the port
	dest.sin_port = *((uint16_t *) p);
	printf("destination port: %d\n", ntohs(dest.sin_port));

    int32_t handle = socket(AF_INET, SOCK_STREAM, 0);
    if(handle < 0){
        printf("error: constructing socket failed\n");
		goto err;
    }

	struct sockaddr_in my_addr;
	socklen_t my_addr_len = sizeof(my_addr);

    int32_t error = connect (handle, (struct sockaddr *) &dest, sizeof (struct sockaddr));

    if(error <0){
        printf("error connecting\n");
		goto err;
    }

	getsockname(handle, (struct sockaddr *) &my_addr, &my_addr_len);

	//now send the reply to the client
	response[0] = 0x05;
	response[1] = 0x00;//TODO: make this accurate
	response[2] = 0x00;
	response[3] = 0x01;
	*((uint32_t *) (response + 4)) = my_addr.sin_addr.s_addr;
	*((uint16_t *) (response + 8)) = my_addr.sin_port;

	printf("Bound to %x:%d\n", my_addr.sin_addr.s_addr, ntohs(my_addr.sin_port));

	bytes_sent = send(sockfd, response, 10,0);
	printf("Sent response (%d bytes):\n", bytes_sent);
	for(int i=0; i< bytes_sent; i++){
		printf("%02x ", response[i]);
	}
	printf("\n");

	//now shuffle data

	for(;;){

		fd_set readfds;
		fd_set writefds;

		int32_t nfds = (sockfd > handle) ? sockfd +1 : handle + 1;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		FD_SET(sockfd, &readfds);
		FD_SET(handle, &readfds);
		FD_SET(sockfd, &writefds);
		FD_SET(handle, &writefds);

		if(select(nfds, &readfds, &writefds, NULL, NULL) <0){
			printf("Select error\n");
			continue;
		}

		if(FD_ISSET(sockfd, &readfds) && FD_ISSET(handle, &writefds)){

			bytes_read = recv(sockfd, buffer, BUFSIZ-1, 0);
			if (bytes_read < 0){
				printf("Error reading from socket\n");
				goto err;
			}

			if(bytes_read > 0){

				printf("Received %d bytes:\n", bytes_read);
				for(int i=0; i< bytes_read; i++){
					printf("%02x ", buffer[i]);
				}
				printf("\n");

				bytes_sent = send(handle, buffer, bytes_read, 0);
				printf("Sent to website (%d bytes):\n", bytes_sent);
				for(int i=0; i< bytes_sent; i++){
					printf("%02x ", buffer[i]);
				}
				printf("\n");

			}
		} else if(FD_ISSET(handle, &readfds) && FD_ISSET(sockfd, &writefds)){

			bytes_read = recv(handle, buffer, BUFSIZ-1, 0);
			if (bytes_read < 0){
				printf("Error reading from socket\n");
				goto err;
			}

			if(bytes_read > 0){

				printf("Received %d bytes:\n", bytes_read);
				for(int i=0; i< bytes_read; i++){
					printf("%02x ", buffer[i]);
				}
				printf("\n");

				bytes_sent = send(sockfd, buffer, bytes_read, 0);
				printf("Sent to website (%d bytes):\n", bytes_sent);
				for(int i=0; i< bytes_sent; i++){
					printf("%02x ", buffer[i]);
				}
				printf("\n");

			}
		}
	}


err:
	close(sockfd);
	free(buffer);
	free(response);
	exit(0);
}
