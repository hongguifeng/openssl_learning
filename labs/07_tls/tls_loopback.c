/*
 * TLS 1.3 内存回环实验：
 *   服务器加载临时证书/私钥；客户端和服务器各有一个 SSL 对象；
 *   BIO_new_bio_pair 把两个端点连接起来；驱动双方握手后交换 ping/pong。
 *
 * 为了把重点放在状态机上，客户端暂时 SSL_VERIFY_NONE。生产代码必须替换为
 * 信任根、主机名和证书用途校验，详见 docs/06-x509.md 和 docs/07-tls.md。
 */
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <stdio.h>
#include <string.h>

static int drive_handshake(SSL *client, SSL *server) {
    /* BIO pair 不会自动调用另一端，所以循环交替推进两个状态机。 */
    for (int i = 0; i < 100; ++i) {
        int cr = SSL_do_handshake(client);
        int ce = cr == 1 ? SSL_ERROR_NONE : SSL_get_error(client, cr);
        int sr = SSL_do_handshake(server);
        int se = sr == 1 ? SSL_ERROR_NONE : SSL_get_error(server, sr);

        if (cr == 1 && sr == 1) {
            return 1;
        }
        /* WANT_READ/WANT_WRITE 是“还需要对端/底层 I/O”，不是致命错误。 */
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
    SSL_CTX *server_ctx = NULL; /* 服务器长期配置 */
    SSL_CTX *client_ctx = NULL; /* 客户端长期配置 */
    SSL *server = NULL;         /* 一次服务器连接 */
    SSL *client = NULL;         /* 一次客户端连接 */
    BIO *client_bio = NULL;
    BIO *server_bio = NULL;
    char received[32] = {0};
    int received_len;
    int ok = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s server.crt server.key\n", argv[0]);
        return 2;
    }

    /* 1. 创建两个独立的 SSL_CTX，明确服务器/客户端角色。 */
    server_ctx = SSL_CTX_new(TLS_server_method());
    client_ctx = SSL_CTX_new(TLS_client_method());
    if (server_ctx == NULL || client_ctx == NULL ||
        /* 2. 实验固定 TLS 1.3，减少协商分支。 */
        SSL_CTX_set_min_proto_version(server_ctx, TLS1_3_VERSION) != 1 ||
        SSL_CTX_set_min_proto_version(client_ctx, TLS1_3_VERSION) != 1 ||
        /* 3. 服务器证书和私钥必须都加载，并确认公钥匹配。 */
        SSL_CTX_use_certificate_chain_file(server_ctx, argv[1]) != 1 ||
        SSL_CTX_use_PrivateKey_file(server_ctx, argv[2], SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(server_ctx) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    /* 教学限制：只演示握手，不演示证书验证；生产代码不能这样配置。 */
    SSL_CTX_set_verify(client_ctx, SSL_VERIFY_NONE, NULL);

    /* 4. 从同一配置创建两个具体连接。 */
    client = SSL_new(client_ctx);
    server = SSL_new(server_ctx);
    if (client == NULL || server == NULL ||
        /* 5. 创建双向内存管道：一个端点给 client，一个给 server。 */
        BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    /* SSL_set_bio 会接管 BIO 所有权；置 NULL 防止 done 路径重复释放。 */
    SSL_set_bio(client, client_bio, client_bio);
    client_bio = NULL;
    SSL_set_bio(server, server_bio, server_bio);
    server_bio = NULL;
    SSL_set_connect_state(client);
    SSL_set_accept_state(server);

    if (!drive_handshake(client, server)) {
        goto done;
    }

    /* 握手成功后，SSL_write/SSL_read 传递的是应用数据，不是 TLS 明文记录。 */
    if (SSL_write(client, "ping", 4) != 4 ||
        (received_len = SSL_read(server, received, 4)) != 4 ||
        memcmp(received, "ping", 4) != 0 ||
        SSL_write(server, "pong", 4) != 4 ||
        (received_len = SSL_read(client, received, 4)) != 4 ||
        memcmp(received, "pong", 4) != 0) {
        (void)received_len;
        ERR_print_errors_fp(stderr);
        goto done;
    }

    printf("TLS %s loopback handshake and ping/pong succeeded\n", SSL_get_version(client));
    printf("cipher: %s\n", SSL_get_cipher_name(client));
    ok = 1;

done:
    BIO_free(client_bio);  /* 只有未交给 SSL 的 BIO 才在这里释放 */
    BIO_free(server_bio);
    SSL_free(client);
    SSL_free(server);
    SSL_CTX_free(client_ctx);
    SSL_CTX_free(server_ctx);
    return ok ? 0 : 1;
}
