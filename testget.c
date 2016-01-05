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

int generate_backdoor_key(SSL *s, DH *dh);

// For this example, we'll be testing on openssl.org
#define SERVER  "cs.uwaterloo.ca"
#define PORT 443

byte key[16];

//Client hello callback
int tag_flow(SSL *s){
	unsigned char *result;
	int len, i;
	FILE *fp;

	result = s->s3->client_random;
	len = sizeof(s->s3->client_random);

	if(len < PTWIST_TAG_BYTES) {
		printf("Uhoh\n");
		return 1;
	}
    //send_time = (s->mode & SSL_MODE_SEND_CLIENTHELLO_TIME) != 0;
    //if (send_time) {
        unsigned long Time = (unsigned long)time(NULL);
        unsigned char *p = result;
        l2n(Time, p);
	tag_hello((byte *) result+4, key);
	printf("Hello tagged.\n");
	fp = fopen("seed", "wb");
	if (fp == NULL) {
		perror("fopen");
		exit(1);
	}
	  for(i=0; i< 16; i++){
	      fprintf(fp, "%02x", key[i]);
	  }
	  fclose(fp);

	//} else {
	//	printf("hmm\n");
	//	tag_hello((byte *) result);
	//}

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
  FILE *fp;

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

	  //Set backdoored DH callback
	  SSL_CTX_set_generate_key_callback(c->sslContext, generate_backdoor_key);
	  SSL_CTX_set_dh_seed(c->sslContext, (unsigned char *) key);

      if (c->sslContext == NULL)
        ERR_print_errors_fp (stderr);

	  //make sure DH is in the cipher list
	  const char *ciphers = "DH";
	  if(!SSL_CTX_set_cipher_list(c->sslContext, ciphers))
		  printf("Failed to set cipher.\n");

      // Create an SSL struct for the connection
      c->sslHandle = SSL_new (c->sslContext);
      if (c->sslHandle == NULL)
        ERR_print_errors_fp (stderr);

	  const unsigned char *list = SSL_get_cipher_list(c->sslHandle, 1);
	  printf("List of ciphers: %s", list);

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

//dh callback
int generate_backdoor_key(SSL *s, DH *dh)
{
    int ok = 0;
    int generate_new_key = 0;
    unsigned l;
    BN_CTX *ctx;
    BN_MONT_CTX *mont = NULL;
    BIGNUM *pub_key = NULL, *priv_key= NULL;
    unsigned char *buf, *seed;
    int bytes, i;
	FILE *fp;

    //seed = s->dh_seed;
	seed = "random";
	printf("In backdoor callback.\n");

    ctx = BN_CTX_new();
    if (ctx == NULL)
        goto err;

    if (dh->priv_key == NULL) {
        priv_key = BN_new();
        if (priv_key == NULL)
            goto err;
        generate_new_key = 1;
    } else
        priv_key = dh->priv_key;

    if (dh->pub_key == NULL) {
        pub_key = BN_new();
        if (pub_key == NULL)
            goto err;
    } else
        pub_key = dh->pub_key;

    if (dh->flags & DH_FLAG_CACHE_MONT_P) {
        mont = BN_MONT_CTX_set_locked(&dh->method_mont_p,
                                      CRYPTO_LOCK_DH, dh->p, ctx);
        if (!mont)
            goto err;
    }

    if (generate_new_key) {
	/* secret exponent length */
	l = dh->length ? dh->length : BN_num_bits(dh->p) - 1;
	bytes = (l+7) / 8;

	/* set exponent to seeded prg value */
	buf = (unsigned char *)OPENSSL_malloc(bytes);
	if (buf == NULL){
	    BNerr(BN_F_BNRAND, ERR_R_MALLOC_FAILURE);
	    goto err;
	}
	RAND_seed(seed, sizeof(seed));

	if(RAND_bytes(buf, bytes) <= 0)
	    goto err;

	//printf("Generated the following rand bytes: ");
	//for(i=0; i< bytes; i++){
	//	printf(" %02x ", buf[i]);
	//}
	//printf("\n");

	if (!BN_bin2bn(buf, bytes, priv_key))
	    goto err;

    }

    {
        BIGNUM local_prk;
        BIGNUM *prk;

        if ((dh->flags & DH_FLAG_NO_EXP_CONSTTIME) == 0) {
            BN_init(&local_prk);
            prk = &local_prk;
            BN_with_flags(prk, priv_key, BN_FLG_CONSTTIME);
        } else
            prk = priv_key;

        if (!dh->meth->bn_mod_exp(dh, pub_key, dh->g, prk, dh->p, ctx, mont))
            goto err;
    }

    dh->pub_key = pub_key;
    dh->priv_key = priv_key;
    ok = 1;
 err:
    if (buf != NULL){
	OPENSSL_cleanse(buf, bytes);
	OPENSSL_free(buf);
    }
    if (ok != 1)
        DHerr(DH_F_GENERATE_KEY, ERR_R_BN_LIB);

    if ((pub_key != NULL) && (dh->pub_key == NULL))
        BN_free(pub_key);
    if ((priv_key != NULL) && (dh->priv_key == NULL))
        BN_free(priv_key);
    BN_CTX_free(ctx);
    return (ok);
}

