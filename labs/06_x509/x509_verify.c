#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509_vfy.h>

#include <stdio.h>

int main(int argc, char **argv) {
    BIO *bio = NULL;
    X509 *cert = NULL;
    X509_STORE *store = NULL;
    X509_STORE_CTX *ctx = NULL;
    X509_VERIFY_PARAM *param = NULL;
    int result = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s cert.pem hostname\n", argv[0]);
        return 2;
    }
    bio = BIO_new_file(argv[1], "rb");
    cert = bio == NULL ? NULL : PEM_read_bio_X509(bio, NULL, NULL, NULL);
    store = X509_STORE_new();
    ctx = X509_STORE_CTX_new();
    if (cert == NULL || store == NULL || ctx == NULL || X509_STORE_add_cert(store, cert) != 1 ||
        X509_STORE_CTX_init(ctx, store, cert, NULL) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    param = X509_STORE_CTX_get0_param(ctx);
    if (X509_VERIFY_PARAM_set1_host(param, argv[2], 0) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    result = X509_verify_cert(ctx);
    if (result == 1) {
        printf("certificate verification succeeded for host %s\n", argv[2]);
    } else {
        int error = X509_STORE_CTX_get_error(ctx);
        printf("certificate verification failed for host %s: %s\n", argv[2],
               X509_verify_cert_error_string(error));
    }
done:
    X509_STORE_CTX_free(ctx);
    X509_STORE_free(store);
    X509_free(cert);
    BIO_free(bio);
    return result == 1 ? 0 : 1;
}

