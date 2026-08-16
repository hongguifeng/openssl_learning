/*
 * EVP 实验：
 *   1. 用 EVP_MD_CTX 计算分块 SHA-256；
 *   2. 用 EVP_CIPHER_CTX 完成 AES-256-GCM 加密/解密；
 *   3. 篡改认证 tag，确认 EVP_DecryptFinal_ex 会拒绝数据。
 *
 * 所有 key/iv 都是固定测试向量，只为让输出可复现，不能用于生产。
 */
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

    if (ctx == NULL) {
        fprintf(stderr, "EVP_MD_CTX_new failed\n");
        goto done;
    }

    /* EVP_sha256 返回算法描述；ctx 保存的是本次计算的可变状态。 */
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        fprintf(stderr, "EVP_DigestInit_ex failed\n");
        goto done;
    }

    /* 故意分成两块，证明 Update 可以重复调用，适合文件/网络流。 */
    if (EVP_DigestUpdate(ctx, message, 5) != 1 ||
        EVP_DigestUpdate(ctx, message + 5, sizeof(message) - 1 - 5) != 1) {
        fprintf(stderr, "EVP_DigestUpdate failed\n");
        goto done;
    }

    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        fprintf(stderr, "EVP_DigestFinal_ex failed\n");
        goto done;
    }
    print_hex("sha256: ", digest, digest_len);
    ok = 1;

done:
    /* 无论哪一步失败，都释放已经创建的上下文。 */
    EVP_MD_CTX_free(ctx);
    if (!ok) {
        ERR_print_errors_fp(stderr);
    }
    return ok;
}

static int aes_gcm_demo(void) {
    /* 固定测试 key/IV：用于复现实验，不是设备密钥。 */
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

    if (ctx == NULL) {
        fprintf(stderr, "EVP_CIPHER_CTX_new failed\n");
        goto done;
    }

    /* 第一次 Init 选择算法；第二次 Init 设置 key 和 IV。 */
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        fprintf(stderr, "GCM encrypt init failed\n");
        goto done;
    }

    /* AAD 不进入 ciphertext，但会被写入 GCM 认证状态。 */
    if (EVP_EncryptUpdate(ctx, NULL, &out_len, aad, (int)(sizeof(aad) - 1)) != 1 ||
        EVP_EncryptUpdate(ctx, ciphertext, &out_len, plaintext,
                          (int)(sizeof(plaintext) - 1)) != 1) {
        fprintf(stderr, "GCM encrypt update failed\n");
        goto done;
    }
    total = out_len;

    /* GCM 通常没有额外 padding，但仍必须调用 Final 完成状态机。 */
    if (EVP_EncryptFinal_ex(ctx, ciphertext + total, &out_len) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, (int)sizeof(tag), tag) != 1) {
        fprintf(stderr, "GCM encrypt final/tag failed\n");
        goto done;
    }
    total += out_len;
    print_hex("gcm ciphertext: ", ciphertext, (size_t)total);
    print_hex("gcm tag: ", tag, sizeof(tag));

    /* 同一个上下文可以 reset，但这里重新创建，突出“一次消息一个状态”。 */
    EVP_CIPHER_CTX_free(ctx);
    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL || EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1 ||
        EVP_DecryptUpdate(ctx, NULL, &out_len, aad, (int)(sizeof(aad) - 1)) != 1 ||
        EVP_DecryptUpdate(ctx, recovered, &out_len, ciphertext, total) != 1) {
        fprintf(stderr, "GCM decrypt update failed\n");
        goto done;
    }

    /* 必须在 DecryptFinal 前设置 tag；Final 的返回值就是认证结果。 */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)sizeof(tag), tag) != 1 ||
        EVP_DecryptFinal_ex(ctx, recovered + out_len, &out_len) != 1) {
        fprintf(stderr, "GCM authentication failed unexpectedly\n");
        goto done;
    }
    recovered_total = (int)(sizeof(plaintext) - 1);
    if (recovered_total != total || memcmp(recovered, plaintext, (size_t)recovered_total) != 0) {
        fprintf(stderr, "recovered plaintext mismatch\n");
        goto done;
    }
    printf("gcm decrypt: %.*s\n", recovered_total, recovered);

    /* 负面测试：tag 被篡改，Final 必须返回失败，明文不可被继续使用。 */
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
    if (!ok) {
        ERR_print_errors_fp(stderr);
    }
    return ok;
}

int main(void) {
    return sha256_demo() && aes_gcm_demo() ? 0 : 1;
}
