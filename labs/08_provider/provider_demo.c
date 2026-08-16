#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/provider.h>

#include <stdio.h>

static void print_hex(const unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", data[i]);
    }
    putchar('\n');
}

int main(void) {
    OSSL_PROVIDER *default_provider = NULL;
    OSSL_PROVIDER *learn_provider = NULL;
    EVP_MD *md = NULL;
    EVP_MD_CTX *ctx = NULL;
    unsigned char digest[64];
    unsigned int digest_len = 0;
    static const unsigned char input[] = "provider dispatch";
    int ok = 0;

    default_provider = OSSL_PROVIDER_load(NULL, "default");
    learn_provider = OSSL_PROVIDER_load(NULL, "learn_provider");
    if (default_provider == NULL || learn_provider == NULL) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    md = EVP_MD_fetch(NULL, "LEARN-TOY-DIGEST", "provider=learn");
    ctx = EVP_MD_CTX_new();
    if (md == NULL || ctx == NULL || EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
        EVP_DigestUpdate(ctx, input, sizeof(input) - 1) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    printf("fetched provider: %s\n", OSSL_PROVIDER_get0_name(EVP_MD_get0_provider(md)));
    printf("toy digest (not cryptographically secure): ");
    print_hex(digest, digest_len);
    ok = digest_len == 32;
done:
    EVP_MD_CTX_free(ctx);
    EVP_MD_free(md);
    OSSL_PROVIDER_unload(learn_provider);
    OSSL_PROVIDER_unload(default_provider);
    return ok ? 0 : 1;
}

