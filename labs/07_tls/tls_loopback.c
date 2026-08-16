#include <openssl/err.h>
#include <openssl/ssl.h>

#include <stdio.h>
#include <string.h>

static int drive_handshake(SSL *client, SSL *server) {
    for (int i = 0; i < 100; ++i) {
        int cr = SSL_do_handshake(client);
        int ce = cr == 1 ? SSL_ERROR_NONE : SSL_get_error(client, cr);
        int sr = SSL_do_handshake(server);
        int se = sr == 1 ? SSL_ERROR_NONE : SSL_get_error(server, sr);
        if (cr == 1 && sr == 1) {
            return 1;
        }
        if ((cr != 1 && ce != SSL_ERROR_WANT_READ && ce != SSL_ERROR_WANT_WRITE) ||
            (sr != 1 && se != SSL_ERROR_WANT_READ && se != SSL_ERROR_WANT_WRITE)) {
            ERR_print_errors_fp(stderr);
            return 0;
        }
    }
    fprintf(stderr, "TLS handshake did not converge\n");
    return 0;
}

int main(int argc, char **argv) {
    SSL_CTX *server_ctx = NULL;
    SSL_CTX *client_ctx = NULL;
    SSL *server = NULL;
    SSL *client = NULL;
    BIO *client_bio = NULL;
    BIO *server_bio = NULL;
    char received[32] = {0};
    int received_len;
    int ok = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s server.crt server.key\n", argv[0]);
        return 2;
    }
    server_ctx = SSL_CTX_new(TLS_server_method());
    client_ctx = SSL_CTX_new(TLS_client_method());
    if (server_ctx == NULL || client_ctx == NULL ||
        SSL_CTX_set_min_proto_version(server_ctx, TLS1_3_VERSION) != 1 ||
        SSL_CTX_set_min_proto_version(client_ctx, TLS1_3_VERSION) != 1 ||
        SSL_CTX_use_certificate_chain_file(server_ctx, argv[1]) != 1 ||
        SSL_CTX_use_PrivateKey_file(server_ctx, argv[2], SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(server_ctx) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    SSL_CTX_set_verify(client_ctx, SSL_VERIFY_NONE, NULL);
    client = SSL_new(client_ctx);
    server = SSL_new(server_ctx);
    if (client == NULL || server == NULL || BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    SSL_set_bio(client, client_bio, client_bio);
    client_bio = NULL;
    SSL_set_bio(server, server_bio, server_bio);
    server_bio = NULL;
    SSL_set_connect_state(client);
    SSL_set_accept_state(server);
    if (!drive_handshake(client, server)) {
        goto done;
    }
    if (SSL_write(client, "ping", 4) != 4 || (received_len = SSL_read(server, received, 4)) != 4 ||
        memcmp(received, "ping", 4) != 0 || SSL_write(server, "pong", 4) != 4 ||
        (received_len = SSL_read(client, received, 4)) != 4 || memcmp(received, "pong", 4) != 0) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    printf("TLS %s loopback handshake and ping/pong succeeded\n", SSL_get_version(client));
    printf("cipher: %s\n", SSL_get_cipher_name(client));
    ok = 1;
done:
    BIO_free(client_bio);
    BIO_free(server_bio);
    SSL_free(client);
    SSL_free(server);
    SSL_CTX_free(client_ctx);
    SSL_CTX_free(server_ctx);
    return ok ? 0 : 1;
}

