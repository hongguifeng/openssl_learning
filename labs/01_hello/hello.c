/*
 * 第一个 C 示例：把“版本、随机数、错误队列、对象释放”放在一个小程序里。
 * 这个文件故意保持简单，重点不是完成业务，而是观察 OpenSSL API 的共同模式。
 */
#include <openssl/crypto.h>  /* OpenSSL_version */
#include <openssl/err.h>     /* ERR_print_errors_fp */
#include <openssl/evp.h>     /* EVP_CIPHER_CTX */
#include <openssl/rand.h>    /* RAND_bytes */
#include <openssl/opensslv.h>

#include <stdio.h>

int main(void) {
    /* RAND_bytes 把随机字节写入调用方提供的缓冲区。 */
    unsigned char bytes[16];

    /* OPENSSL_VERSION_TEXT 来自编译时头文件；OpenSSL_version 来自运行时库。 */
    printf("headers: %s\n", OPENSSL_VERSION_TEXT);
    printf("runtime: %s\n", OpenSSL_version(OPENSSL_VERSION));

    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        /* OpenSSL 失败时通常只返回 0；真正原因在当前线程的错误队列里。 */
        fprintf(stderr, "RAND_bytes failed\n");
        ERR_print_errors_fp(stderr);
        return 1;
    }
    printf("random bytes generated: %zu\n", sizeof(bytes));

    /*
     * 长度为零是一个合法 no-op：函数可以成功，但没有产生任何数据。
     * 这提醒我们不能只看返回值，还要理解函数的语义和实际输出长度。
     */
    if (RAND_bytes(bytes, 0) != 1) {
        fprintf(stderr, "zero-length RAND_bytes unexpectedly failed\n");
        ERR_print_errors_fp(stderr);
        return 1;
    }

    /*
     * 创建一个 EVP 上下文来演示“创建后必须释放”。
     * 这里发送一个不存在的控制命令，故意走错误路径；不要在生产代码中
     * 发送魔数，真正的 EVP 操作应使用文档定义的命令或高层 API。
     */
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL || EVP_CIPHER_CTX_ctrl(ctx, 0x7fffffff, 0, NULL) == 1) {
        fprintf(stderr, "unknown EVP control command was accepted\n");
        EVP_CIPHER_CTX_free(ctx);
        return 1;
    }

    printf("zero-length no-op and error path exercised\n");
    ERR_print_errors_fp(stdout);

    /* free(NULL) 是安全的，所以错误路径也可以统一调用。 */
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}
