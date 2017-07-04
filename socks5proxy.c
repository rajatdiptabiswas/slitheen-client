/*
 * Slitheen - a decoy routing system for censorship resistance
 * Copyright (C) 2017 Cecylia Bocovich (cbocovic@uwaterloo.ca)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Additional permission under GNU GPL version 3 section 7
 * 
 * If you modify this Program, or any covered work, by linking or combining
 * it with the OpenSSL library (or a modified version of that library), 
 * containing parts covered by the terms of the OpenSSL Licence and the
 * SSLeay license, the licensors of this Program grant you additional
 * permission to convey the resulting work. Corresponding Source for a
 * non-source form of such a combination shall include the source code
 * for the parts of the OpenSSL library used as well as that of the covered
 * work.
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
#include <arpa/inet.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/rand.h>

#include "socks5proxy.h"
#include "crypto.h"
#include "tagging.h"
#include "util.h"

#define DEBUG

static connection_table *connections;

typedef struct {
    int32_t in;
    int32_t out;
} ous_pipes;

int main(void){
    int listen_socket;
    
    struct sockaddr_in address;
    struct sockaddr_in remote_addr;
    socklen_t addr_size;

    connections = calloc(1, sizeof(connection_table));
    connections->first = NULL;
    
    int32_t ous_in[2];
    if(pipe(ous_in) < 0){
        printf("Failed to create pipe\n");
        return 1;
    }
    int32_t ous_out[2];
    if(pipe(ous_out) < 0){
        printf("Failed to create pipe\n");
        return 1;
    }

    ous_pipes pipes;
    pipes.in = ous_in[0];
    pipes.out = ous_out[1];

    /* Spawn a thread to communicate with OUS */
    pthread_t *ous_thread = calloc(1, sizeof(pthread_t));
    pthread_create(ous_thread, NULL, ous_IO, (void *) &pipes);

    ous_pipes mux_pipes;
    mux_pipes.in = ous_in[1];
    mux_pipes.out = ous_out[0];

    /* Spawn a thread for multiplexing and demultiplexing */
    pthread_t *demux_thread = calloc(1, sizeof(pthread_t));
    pthread_create(demux_thread, NULL, demultiplex_data, (void *) &mux_pipes);

    pthread_t *mux_thread = calloc(1, sizeof(pthread_t));
    pthread_create(mux_thread, NULL, multiplex_data, (void *) &mux_pipes);

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
        
        new_conn->socket = new_socket;
        new_conn->state = NEW_STREAM;
        new_conn->next = NULL;
        
        if(connections->first == NULL){
            connections->first = new_conn;
            printf("Added first connection with id: %d\n", new_conn->stream_id);
            fflush(stdout);
        } else {
            connection *last = connections->first;
            while(last->next != NULL){
                last = last->next;
            }
            last->next = new_conn;
            printf("Added connection with id: %d at %p\n", new_conn->stream_id, last->next);
            fflush(stdout);
        }

    }

    return 0;
}

/*
 * Responsible for communicating with the OUS. Upstream data is read from the pipes of individual
 * streams and sent to the OUS. Downstream data is read from the OUS, demultiplexed according t
 * stream ID, and sent to the corresponding stream.
 */
void *ous_IO(void *args){

    ous_pipes *pipes = (ous_pipes *) args;

    int32_t ous_in = pipes->in;
    int32_t ous_out = pipes->out;

    //generate Slitheen ID
    uint8_t slitheen_id[SLITHEEN_ID_LEN];
    uint8_t shared_secret[16];

    generate_slitheen_id(slitheen_id, shared_secret);

#ifdef DEBUG
    printf("Randomly generated slitheen id: ");
    int i;
    for(i=0; i< SLITHEEN_ID_LEN; i++){
        printf("%02x ", slitheen_id[i]);
    }
    printf("\n");
#endif

    // Calculate super encryption keys
    generate_super_keys(shared_secret);

    printf("Generated super encrypt keys\n");

    char *encoded_bytes = NULL;
    base64_encode(slitheen_id, SLITHEEN_ID_LEN, &encoded_bytes);

    printf("Encoded ID\n");
    //give encoded slitheen ID to ous
    struct sockaddr_in ous_addr;
    ous_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &(ous_addr.sin_addr));
    ous_addr.sin_port = htons(57173);

    int32_t ous = socket(AF_INET, SOCK_STREAM, 0);
    if(ous < 0){
        printf("Failed to make socket\n");
        pthread_exit(NULL);
    }

    int32_t error = connect(ous, (struct sockaddr *) &ous_addr, sizeof (struct sockaddr));
    if(error < 0){
        printf("Error connecting to OUS\n");
        pthread_exit(NULL);
    }
    printf("Connected to OUS\n");

    uint16_t len = htons(strlen(encoded_bytes));
    int32_t bytes_sent = send(ous, (unsigned char *) &len, sizeof(uint16_t), 0);
    bytes_sent += send(ous, encoded_bytes, ntohs(len), 0);
    printf("Wrote %d bytes to OUS_in: %x\n %s\n", bytes_sent, len, encoded_bytes);

    uint8_t *buffer = emalloc(BUFSIZ);
    int32_t buffer_len = BUFSIZ;

    int32_t bytes_read;

    /* Select on proxy pipes, demux thread, and ous to send and receive data*/
    for(;;){
        fd_set read_fds;
        fd_set write_fds;

        int32_t nfds = ous;
        if(ous_in > nfds)
            nfds = ous_in;
        if(ous_out > nfds)
            nfds = ous_out;


        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        FD_SET(ous_in, &read_fds);
        FD_SET(ous, &read_fds);
        FD_SET(ous_out, &write_fds);
        FD_SET(ous, &write_fds);

        if(select(nfds+1, &read_fds, &write_fds, NULL, NULL) < 0){
            fprintf(stderr, "Select error\n");
            break;
        }

        if(FD_ISSET(ous_in, &read_fds) && FD_ISSET(ous, &write_fds)){

            bytes_read = read(ous_in, buffer, buffer_len);

#ifdef DEBUG
            printf("Received %d bytes from multiplexer\n", bytes_read);
            for(int i=0; i< bytes_read; i++){
                printf("%02x ", buffer[i]);
            }
            printf("\n");
            fflush(stdout);
#endif

            if(bytes_read > 0){
                bytes_sent = send(ous, buffer, bytes_read, 0);
#ifdef DEBUG
                printf("Sent %d bytes to OUS\n", bytes_sent);
                for(int i=0; i< bytes_sent; i++){
                    printf("%02x ", buffer[i]);
                }
                printf("\n");
                fflush(stdout);
#endif

                if(bytes_sent <= 0){
                    fprintf(stderr, "Connection to OUS closed\n");
                    break;
                }

            } else if (bytes_read == 0) {

                fprintf(stderr, "Connection to multiplexer closed\n");
                break;

            } else {
                fprintf(stderr, "Error reading from multiplexer\n");
                break;
            }
        }

        if(FD_ISSET(ous, &read_fds) && FD_ISSET(ous_out, &write_fds)){

            bytes_read = recv(ous, buffer, 4, 0);
#ifdef DEBUG
            printf("Received %d bytes from OUS\n", bytes_read);
            for(int i=0; i< bytes_read; i++){
                printf("%02x ", buffer[i]);
            }
            printf("\n");
            fflush(stdout);
#endif
            if (bytes_read <= 0) {

                fprintf(stderr, "Connection to OUS closed\n");
                break;
            }

            uint32_t *chunk_len = (uint32_t*) buffer;
            
            fprintf(stderr, "Length of this chunk: %u\n", *chunk_len);

            
            bytes_read = recv(ous, buffer, *chunk_len, 0);
#ifdef DEBUG
            printf("Received %d bytes from OUS\n", bytes_read);
            for(int i=0; i< bytes_read; i++){
                printf("%02x ", buffer[i]);
            }
            printf("\n");
            fflush(stdout);
#endif

            if(bytes_read > 0){
                bytes_sent = write(ous_out, buffer, bytes_read);
#ifdef DEBUG
                printf("Sent %d bytes to demultiplexer\n", bytes_sent);
                for(int i=0; i< bytes_sent; i++){
                    printf("%02x ", buffer[i]);
                }
                printf("\n");
                fflush(stdout);
#endif

                if(bytes_sent <= 0){
                    fprintf(stderr, "Connection to demultiplexer closed\n");
                    break;
                }

            } else if (bytes_read == 0) {

                fprintf(stderr, "Connection to OUS closed\n");
                break;

            } else {
                fprintf(stderr, "Error reading from OUS\n");
                break;
            }
        }

    }

    fprintf(stderr, "Closing OUS\n");
    close(ous);
    close(ous_in);
    close(ous_out);
    free(buffer);
    pthread_exit(NULL);

}


/*
 * Continuously read from all stream sockets and pass data to ous
 */
void *multiplex_data(void *args){
    ous_pipes *pipes = (ous_pipes *) args;

    int32_t buffer_len = BUFSIZ;
    uint8_t *buffer = ecalloc(1, buffer_len);

    int32_t bytes_read;

    uint8_t *response = ecalloc(1, BUFSIZ);
    /* Select on stream sockets and ous_in pipe to send and receive data*/
    for(;;){
        fd_set read_fds;
        fd_set write_fds;

        int32_t nfds = 0;

        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);


        //add all stream sockets to read_fds
        connection *conn = connections->first;
        while(conn != NULL){
            if(conn->socket > nfds)
                nfds = conn->socket;
            FD_SET(conn->socket, &read_fds);
            conn = conn->next;
        }

        FD_SET(pipes->in, &write_fds);

        if(pipes->in > nfds)
            nfds = pipes->in;


        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        if(select(nfds+1, &read_fds, &write_fds, NULL, &tv) < 0){
            fprintf(stderr, "Select error\n");
            break;
        }


        struct slitheen_up_hdr *up_hdr;
        uint16_t len;
        char *encoded_bytes = NULL;

        conn = connections->first;
        while(conn != NULL){
            uint8_t stream_id = conn->stream_id;
            if(FD_ISSET(conn->socket, &read_fds) && FD_ISSET(pipes->in, &write_fds)){
                printf("Reading from stream %d\n", conn->stream_id);
                bytes_read = recv(conn->socket, buffer, buffer_len, 0);

                if(bytes_read < 0){
                    close(conn->socket);
                    conn = conn->next;
                    remove_connection(stream_id);
                    continue;
                } else if(bytes_read == 0){
                    //socket is closed
                    printf("Closing connection for stream %d sockfd.\n", conn->stream_id);
                    fflush(stdout);

                    if(conn->state == CONNECTED){
                        //Send close message to slitheen proxy
                        up_hdr = (struct slitheen_up_hdr *) buffer;
                        up_hdr->stream_id = conn->stream_id;
                        up_hdr->len = 0;

                        base64_encode(buffer, 20, &encoded_bytes);

                        len = htons(strlen(encoded_bytes));
                        int32_t bytes_sent = write(pipes->in, (unsigned char *) &len, sizeof(uint16_t));
                        bytes_sent += write(pipes->in, encoded_bytes, ntohs(len));

                        printf("Wrote %d bytes to ous\n", bytes_sent);
                        printf("Closing message: %s\n", encoded_bytes);

                        free(encoded_bytes);
                        encoded_bytes = NULL;
                    }

                    close(conn->socket);
                    conn = conn->next;
                    remove_connection(stream_id);
                    continue;
                }


                switch(conn->state){
                    case NEW_STREAM:
                        printf("Received new stream data from stream %d\n", conn->stream_id);
#ifdef DEBUG
                        printf("Received %d bytes (id %d):\n", bytes_read, conn->stream_id);
                        for(int i=0; i< bytes_read; i++){
                            printf("%02x ", buffer[i]);
                        }
                        printf("\n");
                        fflush(stdout);
#endif

                        //Respond to methods negotiation
                        struct socks_method_req *clnt_meth = (struct socks_method_req *) buffer;
                        uint8_t *p = buffer + 2;

                        if(clnt_meth->version != 0x05){
                            close(conn->socket);
                            printf("Client supplied invalid version: %02x\n", clnt_meth->version);
                            fflush(stdout);
                            conn = conn->next;
                            remove_connection(stream_id);
                            continue;
                        }

                        int responded = 0;
                        int bytes_sent;
                        for(int i=0; i< clnt_meth->num_methods; i++){
                            if(p[0] == 0x00){//send response with METH= 0x00
                                response[0] = 0x05;
                                response[1] = 0x00;
                                send(conn->socket, response, 2, 0);
                                responded = 1;
                            }
                            p++;
                        }
                        if(!responded){//respond with METH= 0xFF
                            response[0] = 0x05;
                            response[1] = 0xFF;
                            send(conn->socket, response, 2, 0);
                            close(conn->socket);
                            conn = conn->next;
                            remove_connection(stream_id);
                            continue;
                        }
                        conn->state = NEGOTIATED;
                        break;
                    case NEGOTIATED:
                        printf("Received negotiation data from stream %d\n", conn->stream_id);
#ifdef DEBUG
                        printf("Received %d bytes (id %d):\n", bytes_read, conn->stream_id);
                        for(int i=0; i< bytes_read; i++){
                            printf("%02x ", buffer[i]);
                        }
                        printf("\n");
                        fflush(stdout);
#endif
                        //Respond to say connection was accepted
                        response[0] = 0x05;
                        response[1] = 0x00;
                        response[2] = 0x00;
                        response[3] = 0x01;

                        *((uint32_t *) (response + 4)) = 0;
                        *((uint16_t *) (response + 8)) = 0;

                        send(conn->socket, response, 10, 0); //TODO: add check for send

                        memmove(buffer+sizeof(struct slitheen_up_hdr), buffer, bytes_read);

                        up_hdr = (struct slitheen_up_hdr *) buffer;
                        up_hdr->stream_id = conn->stream_id;
                        up_hdr->len = htons(bytes_read);

                        bytes_read+= sizeof(struct slitheen_up_hdr);

                        base64_encode(buffer, bytes_read, &encoded_bytes);

                        len = htons(strlen(encoded_bytes));
                        bytes_sent = write(pipes->in, (unsigned char *) &len, sizeof(uint16_t));
                        bytes_sent += write(pipes->in, encoded_bytes, ntohs(len));
                        printf("Wrote %d bytes to ous\n", bytes_sent);

                        free(encoded_bytes);
                        encoded_bytes = NULL;

                        conn->state = CONNECTED;

                        break;
                    case CONNECTED:
                        printf("Received application data from stream %d\n", conn->stream_id);
#ifdef DEBUG_UPSTREAM
                        printf("Received %d data bytes from sockfd (id %d):\n", bytes_read, conn->stream_id);
                        for(i=0; i< bytes_read; i++){
                                printf("%02x ", buffer[i]);
                        }
                        printf("\n");
                        printf("%s\n", buffer);
                        fflush(stdout);
#endif

                        memmove(buffer+sizeof(struct slitheen_up_hdr), buffer, bytes_read);

                        up_hdr = (struct slitheen_up_hdr *) buffer;
                        up_hdr->stream_id = conn->stream_id;
                        up_hdr->len = htons(bytes_read);

                        bytes_read+= sizeof(struct slitheen_up_hdr);

                        base64_encode(buffer, bytes_read, &encoded_bytes);

                        len = htons(strlen(encoded_bytes));
                        bytes_sent = write(pipes->in, (unsigned char *) &len, sizeof(uint16_t));
                        bytes_sent += write(pipes->in, encoded_bytes, ntohs(len));
                        printf("Wrote %d bytes to ous\n", bytes_sent);

#ifdef DEBUG_UPSTREAM
                        printf("Sent to OUS (%d bytes): %x %s\n",bytes_sent, len, encoded_bytes);
#endif
                        free(encoded_bytes);
                        encoded_bytes = NULL;
                        break;
                    default:
                        fprintf(stderr, "Wrong connection state\n");
                        close(conn->socket);
                        conn = conn->next;
                        remove_connection(stream_id);
                        break;
                }

            }

            conn = conn->next;
        }
    }
    free(response);
    pthread_exit(NULL);
}


/* Read blocks of covert data from the OUS. Determine the stream id and the length of
 * the block and then write the data to the correct thread to be passed to the browser
 */
void *demultiplex_data(void *args){
    ous_pipes *pipes = (ous_pipes *) args;

    int32_t buffer_len = BUFSIZ;
    uint8_t *buffer = calloc(1, buffer_len);
    uint8_t *p;

    uint8_t *partial_block = NULL;
    uint32_t partial_block_len = 0;
    uint32_t resource_remaining = 0;
    uint64_t expected_next_count = 1;
    data_block *saved_data = NULL;

    for(;;){
        printf("Demux thread waiting to read\n");
        int32_t bytes_read = read(pipes->out, buffer, buffer_len-partial_block_len);
		
        if(bytes_read > 0){
            int32_t bytes_remaining = bytes_read;
            p = buffer;

            //didn't read a full slitheen block last time
            if(partial_block_len > 0){
                //process first part of slitheen info
                memmove(buffer+partial_block_len, buffer, bytes_read);
                memcpy(buffer, partial_block, partial_block_len);
                bytes_remaining += partial_block_len;
                free(partial_block);
                partial_block = NULL;
                partial_block_len = 0;
            }

            while(bytes_remaining > 0){
                if(resource_remaining <= 0){//we're at a new resource
                    //the first value for a new resource will be the resource length,
                    //followed by a newline
                    uint8_t *end_ptr;
                    resource_remaining = strtol((const char *) p, (char **) &end_ptr, 10);
#ifdef DEBUG_PARSE
                    printf("Starting new resource of len %d bytes\n", resource_remaining);
                    printf("Resource len bytes:\n");
                    int i;
                    for(i=0; i< (end_ptr - p) + 1; i++){
                        printf("%02x ", ((const char *) p)[i]);
                    }
                    printf("\n");
#endif
                    if(resource_remaining == 0){
                        bytes_remaining -= (end_ptr - p) + 1;
                        p += (end_ptr - p) + 1;
                    } else {
                        bytes_remaining -= (end_ptr - p) + 1;
                        p += (end_ptr - p) + 1;

                    }
                    continue;

                }


                if(resource_remaining < SLITHEEN_HEADER_LEN){
                    printf("ERROR: Resource remaining doesn't fit header len.\n");
                    resource_remaining = 0;
                    bytes_remaining = 0;
                    break;
                }

                if(bytes_remaining < SLITHEEN_HEADER_LEN){

#ifdef DEBUG_PARSE
                    printf("Partial header: ");
                    int i;
                    for(i = 0; i< bytes_remaining; i++){
                        printf("%02x ", p[i]);
                    }
                    printf("\n");
#endif

                    if(partial_block != NULL) printf("UH OH (PB)\n");
                    partial_block = calloc(1, bytes_remaining);
                    memcpy(partial_block, p, bytes_remaining);
                    partial_block_len = bytes_remaining;
                    bytes_remaining = 0;
                    break;
                }

                //decrypt header to see if we have entire block
                uint8_t *tmp_header = malloc(SLITHEEN_HEADER_LEN);
                memcpy(tmp_header, p, SLITHEEN_HEADER_LEN);
                peek_header(tmp_header);

                struct slitheen_hdr *sl_hdr = (struct slitheen_hdr *) tmp_header;
                //first see if sl_hdr corresponds to a valid stream. If not, ignore rest of read bytes
#ifdef DEBUG_PARSE
                printf("Slitheen header:\n");
                int i;
                for(i = 0; i< SLITHEEN_HEADER_LEN; i++){
                    printf("%02x ", tmp_header[i]);
                }
                printf("\n");
#endif
                if(ntohs(sl_hdr->len) > resource_remaining){
                    printf("ERROR: slitheen block doesn't fit in resource remaining!\n");
                    resource_remaining = 0;
                    bytes_remaining = 0;
                    break;
                }

                if(ntohs(sl_hdr->len) > bytes_remaining){
                    if(partial_block != NULL) printf("UH OH (PB)\n");
                    partial_block = calloc(1, ntohs(sl_hdr->len));
                    memcpy(partial_block, p, bytes_remaining);
                    partial_block_len = bytes_remaining;
                    bytes_remaining = 0;
                    free(tmp_header);
                    break;
                }

                super_decrypt(p);

                sl_hdr = (struct slitheen_hdr *) p;
                free(tmp_header);

                p += SLITHEEN_HEADER_LEN;
                bytes_remaining -= SLITHEEN_HEADER_LEN;
                resource_remaining -= SLITHEEN_HEADER_LEN;

                if((!sl_hdr->len) && (sl_hdr->garbage)){

#ifdef DEBUG_PARSE
                    printf("%d Garbage bytes\n", ntohs(sl_hdr->garbage));
#endif
                    p += ntohs(sl_hdr->garbage);
                    bytes_remaining -= ntohs(sl_hdr->garbage);
                    resource_remaining -= ntohs(sl_hdr->garbage);
                    continue;
                }

                int32_t sock =-1;
                if(connections->first == NULL){
                    printf("Error: there are no connections\n");
                } else {
                    connection *last = connections->first;
                    if (last->stream_id == sl_hdr->stream_id){
                        sock = last->socket;
                    }
                    while(last->next != NULL){
                        last = last->next;
                        if (last->stream_id == sl_hdr->stream_id){
                            sock = last->socket;
                        }
                    }
                }
				
                if(sock == -1){
                    printf("No stream id exists. Possibly invalid header\n");
                    break;
                }
				
#ifdef DEBUG_PARSE
                printf("Received information for stream id: %d of length: %u\n", sl_hdr->stream_id, ntohs(sl_hdr->len));
#endif

                //figure out how much to skip
                int32_t padding = 0;
                if(ntohs(sl_hdr->len) %16){
                    padding = 16 - ntohs(sl_hdr->len)%16;
                }
                p += 16; //IV

                //check counter to see if we are missing data
                if(sl_hdr->counter > expected_next_count){
                    //save any future data
                    printf("Received header with count %lu. Expected count %lu.\n",
                                        sl_hdr->counter, expected_next_count);
                    if((saved_data == NULL) || (saved_data->count > sl_hdr->counter)){
                        data_block *new_block = malloc(sizeof(data_block));
                        new_block->count = sl_hdr->counter;
                        new_block->len = ntohs(sl_hdr->len);
                        new_block->data = malloc(ntohs(sl_hdr->len));

                        memcpy(new_block->data, p, ntohs(sl_hdr->len));

                        new_block->socket = sock;
                        new_block->next = saved_data;

                        saved_data = new_block;

                    } else {
                        data_block *last = saved_data;
                        while((last->next != NULL) && (last->next->count < sl_hdr->counter)){
                            last = last->next;
                        }
                        data_block *new_block = malloc(sizeof(data_block));
                        new_block->count = sl_hdr->counter;
                        new_block->len = ntohs(sl_hdr->len);
                        new_block->data = malloc(ntohs(sl_hdr->len));
                        memcpy(new_block->data, p, ntohs(sl_hdr->len));
                        new_block->socket = sock;
                        new_block->next = last->next;

                        last->next = new_block;
                    }
                } else {
                    int32_t bytes_sent = send(sock, p, ntohs(sl_hdr->len), 0);
                    if(bytes_sent <= 0){
                        printf("Error writing to socket for stream id %d\n", sl_hdr->stream_id);
                    }

                    //increment expected counter
                    expected_next_count++;
                }

                //now check to see if there is saved data to write out
                if(saved_data != NULL){
                    data_block *current_block = saved_data;
                    while((current_block != NULL) && (expected_next_count == current_block->count)){
                        int32_t bytes_sent = send(current_block->socket, current_block->data, 
                                current_block->len, 0);
                        if(bytes_sent <= 0){
                            printf("Error writing to socket for stream id %d\n", sl_hdr->stream_id);
                        }
                        expected_next_count++;
                        saved_data = current_block->next;
                        free(current_block->data);
                        free(current_block);
                        current_block = saved_data;
                    }
                }

                p += ntohs(sl_hdr->len); //encrypted data
                p += 16; //mac
                p += padding;
                p += ntohs(sl_hdr->garbage);

                bytes_remaining -= ntohs(sl_hdr->len) + 16 + padding + 16 + ntohs(sl_hdr->garbage);
                resource_remaining -= ntohs(sl_hdr->len) + 16 + padding + 16 + ntohs(sl_hdr->garbage);

            }

        } else {
            printf("Error: read %d bytes from OUS_out\n", bytes_read);
            goto err;
        }
		
    }
err:
    free(buffer);
    close(pipes->out);
    pthread_exit(NULL);

}


int remove_connection(uint16_t stream_id){

    connection *last = connections->first;
    connection *prev = last;
    while(last != NULL){
        if(last->stream_id == stream_id){
            if(last == connections->first){
                connections->first = last->next;
            } else {
                prev->next = last->next;
            }
            free(last);
            printf("Removed stream id %d from connections table\n", stream_id);
            break;
        }

        prev = last;
        last = last->next;
    }

    return 1;
}


