#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/wait.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/buffer.h>

#define HOST "10.0.2.2"
#define PORT "29170"
#define QUOTE_COMMAND "/usr/bin/trustauthority-cli"
#define BUF_SIZE 1024
#define QUOTE_LENGTH_V4 10000
//#define CERTIFICATE "../Certificate/CA_cert.pem"
#define CERTIFICATE "CA_cert.pem"

#define DEBUG 1

int generateQuote(char *nonce, char *quote);
void pexit(char *);
void pdebug(char *);

void pexit(char *msg){
    fprintf(stderr, "[ERROR]: %s\n", msg);
    exit(EXIT_FAILURE);
}

void pdebug(char *msg){
    if (DEBUG) fprintf(stderr, "[DEBUG]: %s\n", msg);
}

char *to_base64(const unsigned char *data, size_t len){
    BIO *bio_b64 = NULL;
    BIO *bio_mem = NULL;
    BUF_MEM *buffer_ptr = NULL;

    bio_b64 = BIO_new(BIO_f_base64());
    if(bio_b64 == NULL)
        return NULL;
    
    bio_mem = BIO_new(BIO_s_mem());
    if(bio_mem == NULL)
        return NULL;
    
    bio_b64 = BIO_push(bio_b64, bio_mem);
    BIO_set_flags(bio_b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio_b64, data, len);
    BIO_flush(bio_b64);

    BIO_get_mem_ptr(bio_b64, &buffer_ptr);
    
    char *b64text = (char *) malloc(buffer_ptr->length + 1);
    if(b64text == NULL)
        return NULL;

    memcpy(b64text, buffer_ptr->data, buffer_ptr->length);
    b64text[buffer_ptr->length] = '\0';
    BIO_free_all(bio_b64);

    return b64text;
}

int main(int argc, char const *argv[]){

    /* init OpenSSL context */
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if(ctx == NULL) pexit("SSL_CTX object could not be created");

    /* enable certificate verification of the server */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    /* sets the path to where the trusted certificates are stored */
    if(!SSL_CTX_load_verify_locations(ctx, CERTIFICATE, NULL)){
        pexit("Could not find certificate file");
    }

    /* set minimum required TLS version */
    if(!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION))
        pexit("Could not set minimum TLS version");
    
    /* Create SSL object */
    SSL *ssl = SSL_new(ctx);
    if(ssl == NULL) pexit("Failed to create SSL object");

    /* Tries to connect a socket using OpenSSL BIO functions */
    int sock = -1;
    BIO_ADDRINFO *res;
    const BIO_ADDRINFO *ai = NULL;

    if(!BIO_lookup_ex(HOST, PORT, BIO_LOOKUP_CLIENT, AF_INET, SOCK_STREAM, 0, &res)) pexit("Failed to get address info");
    /* This is similar to getaddrinfo(3) */
    for (ai = res; ai != NULL; ai = BIO_ADDRINFO_next(ai)){
        sock = BIO_socket(BIO_ADDRINFO_family(ai), SOCK_STREAM, 0, 0);
        if(sock == -1) continue;

        if(BIO_connect(sock, BIO_ADDRINFO_address(ai), BIO_SOCK_NODELAY)) break;

        if(!BIO_closesocket(sock)) pexit("Failed to close socket");
        sock = -1;
    }
    BIO_ADDRINFO_free(res);

    /* Associate socket with BIO object */
    BIO *bio = BIO_new(BIO_s_socket());
    if(bio == NULL) pexit("Faile to create BIO object");
    
    if(BIO_set_fd(bio, sock, BIO_CLOSE) <= 0) pexit("Could not bind BIO object to socket");

    /* DO NOT FREE BIO object after this step -> will be freed when SSL is freed */
    SSL_set_bio(ssl, bio, bio);

    if(!SSL_set_tlsext_host_name(ssl, HOST)) pexit("Could not set TLS_EXT hostname");

    if(!SSL_set1_host(ssl, HOST)) pexit("Failed to set the certificate verification hostname");

    /* TLS Handshake */
    if(SSL_connect(ssl) < 1){
        if(SSL_get_verify_result(ssl) != X509_V_OK){
            fprintf(stderr, "Verify error: %s\n", X509_verify_cert_error_string(SSL_get_verify_result(ssl)));
        }
        pexit("Failed to connect to server");
    }

    /* Start data transfer */
    size_t written;

    /* Request nonce for quote generation */
    const char *request_nonce = "GET NONCE";
    if(!SSL_write_ex(ssl, request_nonce, strlen(request_nonce), &written)){
        pexit("Failed to send GET NONCE request");
    }




    /* Receive nonce from server */
    size_t readbytes;
    char buf[BUF_SIZE];
    if(SSL_read_ex(ssl, buf, BUF_SIZE, &readbytes) <= 0){
        if(SSL_get_error(ssl, 0) != SSL_ERROR_ZERO_RETURN) pexit("No nonce received!");
    }
    if(readbytes >= BUF_SIZE)
        pexit("to many bytes read, bufferoverflow");
    else buf[readbytes] = '\0';
    char *nonce = buf;

    
    unsigned char ekm[32];
    const char *label = "EXPORTER-TEE-TLS-BINDING";
    if (!SSL_export_keying_material(
        ssl,
        ekm,
        sizeof(ekm),
        label,
        strlen(label),
        nonce,
        strlen(nonce),
        1)){
	    pexit("Failed to export TLS keying material");
	}

    char *ekm_b64 = to_base64(ekm, sizeof(ekm));

    
    char *quote = (char *) malloc(QUOTE_LENGTH_V4);
    if(quote == NULL)
       pexit("malloc failed");

    int recvpipefd[2]; /* file descriptors for the pipe used for receiving the quote */

    if(pipe(recvpipefd) == -1)
	    pexit("Failed to create pipe");
    
    /* Create new process for quote generation */
    pid_t cpid;
    cpid = fork();
    if (cpid == -1){
    	pexit("Failed to fork process");
    } else if (cpid == 0){
    	/* Child process */
	    /* Child writes on recvpipefd -> close unused read fd */
	    close(recvpipefd[0]);
	    dup2(recvpipefd[1], 1); /* send stdout to the pipe */
        close(recvpipefd[1]);
	    /* Generate Quote */
	    char *argv[] = {QUOTE_COMMAND, "quote", "-n", ekm_b64, (char) 0};
	    if(execve(QUOTE_COMMAND, argv, NULL) == -1)
		    pexit("execve failed");

	    /* End child process */
    } else {
    	/* Parent process */
	    close(recvpipefd[1]);

        /* Check if execve failed */
        int status;

	    /* Receive generated quote */
        ssize_t bytes = read(recvpipefd[0], quote, QUOTE_LENGTH_V4);
	    if(bytes == -1)
		    pexit("read from pipe failed");

	    /* Closing pipes -> EOF */;
	    close(recvpipefd[0]);

	    pid_t result = waitpid(cpid, &status, 0);
        if(result == -1)
            pexit("Child error");
        
        if(bytes == 0)
            pexit("Empty Quote");
    }
    /*
    if(generateQuote(nonce, quote) == -1){
    	pexit("Quote generation failed!");
    }
    */

    /* Sending Quote */
    if(!SSL_write_ex(ssl, quote, strlen(quote), &written)){
        pexit("Failed to send TDX Quote");
    }
    free(quote);

    /* Receive decryption key or ERROR */
    if(SSL_read_ex(ssl, buf, BUF_SIZE, &readbytes) <= 0){
        if(SSL_get_error(ssl, 0) != SSL_ERROR_ZERO_RETURN) pexit("No decryption key received!");
    }


    if(readbytes > BUF_SIZE)
        pexit("to many bytes read, buffer overflow");
    else buf[readbytes] = '\0';
    /* Server will send 'ERROR' if quote was not verified */
    if(strcmp(buf, "ERROR") == 0){
        /* TODO: should poweroff TD */
        pexit("TDX Quote could not be verified something went wrong");
    }

    fprintf(stderr, "Received password: %s\n", buf);
    
    printf("%s", buf);
    EVP_cleanup();
    ERR_free_strings();
    /* End connection */
    int ret = SSL_shutdown(ssl);
    exit(EXIT_SUCCESS);
}

