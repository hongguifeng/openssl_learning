/*
 * X.509 实验：
 *   从 PEM 读取证书 → 把它作为实验信任根 → 设置期望主机名
 *   → 执行链/签名/名称验证。
 *
 * run_x509_lab.sh 会用同一张证书测试 localhost（成功）和 wrong.example（失败）。
 */
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509_vfy.h>

#include <stdio.h>

int main(int argc, char **argv) {
    BIO *bio = NULL;                 /* 输入证书文件 */
    X509 *cert = NULL;               /* 解析后的证书对象 */
    X509_STORE *store = NULL;        /* 信任根集合 */
    X509_STORE_CTX *ctx = NULL;      /* 一次验证的临时上下文 */
    X509_VERIFY_PARAM *param = NULL; /* ctx 内的验证策略 */
    int result = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s cert.pem hostname\n", argv[0]);
        return 2;
    }

    /* PEM_read 只负责“能否解析”；它本身不代表证书可信。 */
    bio = BIO_new_file(argv[1], "rb");
    cert = bio == NULL ? NULL : PEM_read_bio_X509(bio, NULL, NULL, NULL);
    store = X509_STORE_new();
    ctx = X509_STORE_CTX_new();
    if (cert == NULL || store == NULL || ctx == NULL ||
        /* 仅为本地实验把同一张自签名证书加入 trust store。 */
        X509_STORE_add_cert(store, cert) != 1 ||
        X509_STORE_CTX_init(ctx, store, cert, NULL) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    /* 验证策略必须显式设置期望主机名，不能只看 CN。 */
    param = X509_STORE_CTX_get0_param(ctx);
    if (X509_VERIFY_PARAM_set1_host(param, argv[2], 0) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    /* 这里会检查信任链、证书签名、有效期及主机名。 */
    result = X509_verify_cert(ctx);
    if (result == 1) {
        printf("certificate verification succeeded for host %s\n", argv[2]);
    } else {
        int error = X509_STORE_CTX_get_error(ctx);
        printf("certificate verification failed for host %s: %s\n", argv[2],
               X509_verify_cert_error_string(error));
    }

done:
    /* ctx 不拥有 store/cert 的长期所有权，分别释放。 */
    X509_STORE_CTX_free(ctx);
    X509_STORE_free(store);
    X509_free(cert);
    BIO_free(bio);
    return result == 1 ? 0 : 1;
}
