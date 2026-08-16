#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>

#include <stdio.h>
#include <string.h>

int main(void) {
    static const unsigned char message[] = "key serialization demo";
    EVP_PKEY_CTX *keygen = NULL;
    EVP_PKEY *key = NULL;
    EVP_PKEY *loaded = NULL;
    EVP_MD_CTX *sign_ctx = NULL;
    BIO *memory = NULL;
    unsigned char signature[128];
    size_t signature_len = sizeof(signature);
    int ok = 0;

    keygen = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (keygen == NULL || EVP_PKEY_keygen_init(keygen) != 1 ||
        EVP_PKEY_CTX_set_ec_paramgen_curve_nid(keygen, NID_X9_62_prime256v1) != 1 ||
        EVP_PKEY_keygen(keygen, &key) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    memory = BIO_new(BIO_s_mem());
    if (memory == NULL || PEM_write_bio_PrivateKey(memory, key, NULL, NULL, 0, NULL, NULL) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    loaded = PEM_read_bio_PrivateKey(memory, NULL, NULL, NULL);
    if (loaded == NULL) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    sign_ctx = EVP_MD_CTX_new();
    if (sign_ctx == NULL || EVP_DigestSignInit(sign_ctx, NULL, EVP_sha256(), NULL, loaded) != 1 ||
        EVP_DigestSignUpdate(sign_ctx, message, sizeof(message) - 1) != 1 ||
        EVP_DigestSignFinal(sign_ctx, signature, &signature_len) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    EVP_MD_CTX_reset(sign_ctx);
    if (EVP_DigestVerifyInit(sign_ctx, NULL, EVP_sha256(), NULL, key) != 1 ||
        EVP_DigestVerifyUpdate(sign_ctx, message, sizeof(message) - 1) != 1 ||
        EVP_DigestVerifyFinal(sign_ctx, signature, signature_len) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    printf("EC key generated, serialized, reloaded and verified (%zu-byte signature)\n", signature_len);
    ok = 1;
done:
    if (!ok) {
        ERR_print_errors_fp(stderr);
    }
    EVP_MD_CTX_free(sign_ctx);
    EVP_PKEY_free(loaded);
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(keygen);
    BIO_free(memory);
    return ok ? 0 : 1;
}

