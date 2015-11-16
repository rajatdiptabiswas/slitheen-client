#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "ptwist.h"
#include "rclient.h"
/* Copied from ssl_locl.h */
# define l2n(l,c)        (*((c)++)=(unsigned char)(((l)>>24)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>16)&0xff), \
                         *((c)++)=(unsigned char)(((l)>> 8)&0xff), \
                         *((c)++)=(unsigned char)(((l)    )&0xff))

// Simple structure to keep track of the handle, and
// of what needs to be freed later.
typedef struct {
    int socket;
    SSL *sslHandle;
    SSL_CTX *sslContext;
} connection;

// For this example, we'll be testing on openssl.org
#define SERVER  "cs.uwaterloo.ca"
#define PORT 443

int tag_flow(SSL *s){
	unsigned char *result;
	int len;
    int send_time = 0;

	result = s->s3->client_random;
	len = sizeof(s->s3->client_random);

	if(len < PTWIST_TAG_BYTES) {
		printf("Uhoh\n");
		return 1;
	}
    send_time = (s->mode & SSL_MODE_SEND_CLIENTHELLO_TIME) != 0;
    if (send_time) {
        unsigned long Time = (unsigned long)time(NULL);
        unsigned char *p = result;
        l2n(Time, p);
		tag_hello((byte *) result+4);
	} else {
		printf("hmm\n");
		tag_hello((byte *) result);
	}

	return 0;
}

// Establish a regular tcp connection
int tcpConnect ()
{
  int error, handle;
  struct hostent *host;
  struct sockaddr_in server;

  host = gethostbyname (SERVER);
  handle = socket (AF_INET, SOCK_STREAM, 0);
  if (handle == -1)
    {
      perror ("Socket");
      handle = 0;
    }
  else
    {
      server.sin_family = AF_INET;
      server.sin_port = htons (PORT);
      server.sin_addr = *((struct in_addr *) host->h_addr);
      bzero (&(server.sin_zero), 8);

      error = connect (handle, (struct sockaddr *) &server,
                       sizeof (struct sockaddr));
      if (error == -1)
        {
          perror ("Connect");
          handle = 0;
        }
    }

  return handle;
}

// Establish a connection using an SSL layer
connection *sslConnect (void)
{
  connection *c;

  c = malloc (sizeof (connection));
  c->sslHandle = NULL;
  c->sslContext = NULL;

  c->socket = tcpConnect ();
  if (c->socket)
    {
      // Register the error strings for libcrypto & libssl
      SSL_load_error_strings();
      // Register the available ciphers and digests
      SSL_library_init();

      // New context saying we are a client, and using TLSv1.2
      c->sslContext = SSL_CTX_new (TLSv1_2_method());

	  //Tag the client hello message with Telex tag
	  SSL_CTX_set_client_hello_callback(c->sslContext, tag_flow);
      if (c->sslContext == NULL)
        ERR_print_errors_fp (stderr);

      // Create an SSL struct for the connection
      c->sslHandle = SSL_new (c->sslContext);
      if (c->sslHandle == NULL)
        ERR_print_errors_fp (stderr);

      // Connect the SSL struct to our connection
      if (!SSL_set_fd (c->sslHandle, c->socket))
        ERR_print_errors_fp (stderr);

      // Initiate SSL handshake
      if (SSL_connect (c->sslHandle) != 1)
        ERR_print_errors_fp (stderr);
    }
  else
    {
      perror ("Connect failed");
    }

  return c;
}

// Disconnect & free connection struct
void sslDisconnect (connection *c)
{
  if (c->socket)
    close (c->socket);
  if (c->sslHandle)
    {
      SSL_shutdown (c->sslHandle);
      SSL_free (c->sslHandle);
    }
  if (c->sslContext)
    SSL_CTX_free (c->sslContext);

  free (c);
}

// Read all available text from the connection
char *sslRead (connection *c)
{
  const int readSize = 1024;
  char *rc = NULL;
  int received, count = 0;
  char buffer[1024];

  if (c)
    {
      while (1)
        {
          if (!rc)
            rc = malloc (readSize * sizeof (char) + 1);
          else
            rc = realloc (rc, (count + 1) *
                          readSize * sizeof (char) + 1);

          received = SSL_read (c->sslHandle, buffer, readSize);
          buffer[received] = '\0';

          if (received > 0)
            strcat (rc, buffer);

          if (received < readSize)
            break;
          count++;
        }
    }

  return rc;
}

// Write text to the connection
void sslWrite (connection *c, char *text)
{
  if (c)
    SSL_write (c->sslHandle, text, strlen (text));
  printf("Wrote:\n%s\n", text);
}

// Very basic main: we send GET / and print the response.
int main (int argc, char **argv)
{
  connection *c;
  char *response;

  c = sslConnect ();

  sslWrite (c, "GET /index.php HTTP/1.1\r\nhost: cs.uwaterloo.ca\r\n\r\n");
  response = sslRead (c);

  printf ("%s\n", response);

  sslDisconnect (c);
  free (response);

  return 0;
}
