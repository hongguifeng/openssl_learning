#include <openssl/evp.h>
#include <openssl/err.h>

#include <stdio.h>
#include <string.h>

static void print_hex(const char *label, const unsigned char *data, size_t len) {
    printf("%s", label);
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", data[i]);
    }
    putchar('\n');
}

static int sha256_demo(void) {
    static const unsigned char message[] = "openssl-evp-demo";
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    int ok = 0;

    if (ctx == NULL || EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, message, sizeof(message) - 1) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    print_hex("sha256: ", digest, digest_len);
    ok = 1;
done:
    EVP_MD_CTX_free(ctx);
    return ok;
}

static int aes_gcm_demo(void) {
    static const unsigned char key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };
    static const unsigned char iv[12] = {
        0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5,
        0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab
    };
    static const unsigned char aad[] = "header";
    static const unsigned char plaintext[] = "embedded tls payload";
    unsigned char ciphertext[sizeof(plaintext)];
    unsigned char recovered[sizeof(plaintext)];
    unsigned char tag[16];
    int out_len = 0;
    int total = 0;
    int recovered_total = 0;
    int ok = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (ctx == NULL || EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1 ||
        EVP_EncryptUpdate(ctx, NULL, &out_len, aad, (int)(sizeof(aad) - 1)) != 1 ||
        EVP_EncryptUpdate(ctx, ciphertext, &out_len, plaintext, (int)(sizeof(plaintext) - 1)) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    total = out_len;
    if (EVP_EncryptFinal_ex(ctx, ciphertext + total, &out_len) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    total += out_len;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, (int)sizeof(tag), tag) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    print_hex("gcm ciphertext: ", ciphertext, (size_t)total);
    print_hex("gcm tag: ", tag, sizeof(tag));
    EVP_CIPHER_CTX_free(ctx);
    ctx = NULL;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL || EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1 ||
        EVP_DecryptUpdate(ctx, NULL, &out_len, aad, (int)(sizeof(aad) - 1)) != 1 ||
        EVP_DecryptUpdate(ctx, recovered, &out_len, ciphertext, total) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)sizeof(tag), tag) != 1 ||
        EVP_DecryptFinal_ex(ctx, recovered + out_len, &out_len) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }
    recovered_total = (int)(sizeof(plaintext) - 1);
    if (recovered_total != total || memcmp(recovered, plaintext, (size_t)recovered_total) != 0) {
        fprintf(stderr, "recovered plaintext mismatch\n");
        goto done;
    }
    printf("gcm decrypt: %.*s\n", recovered_total, recovered);

    /* 认证失败必须被当作失败，而不是继续使用输出。 */
    tag[0] ^= 1U;
    EVP_CIPHER_CTX_free(ctx);
    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL || EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1 ||
        EVP_DecryptUpdate(ctx, NULL, &out_len, aad, (int)(sizeof(aad) - 1)) != 1 ||
        EVP_DecryptUpdate(ctx, recovered, &out_len, ciphertext, total) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)sizeof(tag), tag) != 1 ||
        EVP_DecryptFinal_ex(ctx, recovered + out_len, &out_len) == 1) {
        fprintf(stderr, "tampered tag was accepted\n");
        goto done;
    }
    printf("tamper test: authentication failure detected\n");
    ok = 1;
done:
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

int main(void) {
    return sha256_demo() && aes_gcm_demo() ? 0 : 1;
}
