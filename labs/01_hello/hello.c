#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>

#include <stdio.h>

int main(void) {
    unsigned char bytes[16];

    printf("headers: %s\n", OPENSSL_VERSION_TEXT);
    printf("runtime: %s\n", OpenSSL_version(OPENSSL_VERSION));

    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        fprintf(stderr, "RAND_bytes failed\n");
        ERR_print_errors_fp(stderr);
        return 1;
    }

    printf("random bytes generated: %zu\n", sizeof(bytes));

    /* 零长度请求是合法 no-op；真正的错误路径用未知 EVP 控制命令演示。 */
    if (RAND_bytes(bytes, 0) != 1) {
        fprintf(stderr, "zero-length RAND_bytes unexpectedly failed\n");
        ERR_print_errors_fp(stderr);
        return 1;
    }
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL || EVP_CIPHER_CTX_ctrl(ctx, 0x7fffffff, 0, NULL) == 1) {
        fprintf(stderr, "unknown EVP control command was accepted\n");
        EVP_CIPHER_CTX_free(ctx);
        return 1;
    }
    printf("zero-length no-op and error path exercised\n");
    ERR_print_errors_fp(stdout);
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}
