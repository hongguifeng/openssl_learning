/*
 * 密钥实验：
 *   生成 P-256 EC 密钥 → 写成 PEM → 从 BIO 读回 → 签名/验签。
 *
 * 所有数据都在内存中，实验不会把私钥写入仓库。
 */
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>

#include <stdio.h>

int main(void) {
    static const unsigned char message[] = "key serialization demo";
    EVP_PKEY_CTX *keygen = NULL; /* 临时的“生成密钥”操作上下文 */
    EVP_PKEY *key = NULL;        /* 第一次生成的密钥对象 */
    EVP_PKEY *loaded = NULL;     /* 从 PEM 读回的第二个密钥对象 */
    EVP_MD_CTX *sign_ctx = NULL; /* 一次签名/验签操作的摘要上下文 */
    BIO *memory = NULL;          /* 内存 BIO：保存 PEM 文本 */
    unsigned char signature[128];
    size_t signature_len = sizeof(signature);
    int ok = 0;

    /* 1. 创建 EC 密钥生成上下文；keygen 不拥有最终 key。 */
    keygen = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (keygen == NULL || EVP_PKEY_keygen_init(keygen) != 1 ||
        /* 2. 设置曲线参数。NID 是 OpenSSL 内部对 P-256 的命名方式。 */
        EVP_PKEY_CTX_set_ec_paramgen_curve_nid(keygen, NID_X9_62_prime256v1) != 1 ||
        /* 3. 生成结果写入 key，成功后由应用负责 EVP_PKEY_free。 */
        EVP_PKEY_keygen(keygen, &key) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    /* 4. 内存 BIO 像一个可读写文件；PEM_write 会把私钥写进去。 */
    memory = BIO_new(BIO_s_mem());
    if (memory == NULL ||
        PEM_write_bio_PrivateKey(memory, key, NULL, NULL, 0, NULL, NULL) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    /* 5. 从 BIO 解析 PEM，得到独立的 EVP_PKEY 对象。 */
    loaded = PEM_read_bio_PrivateKey(memory, NULL, NULL, NULL);
    if (loaded == NULL) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    /* 6. 使用“读回的私钥”签名。这里 EVP_sha256 是摘要算法描述。 */
    sign_ctx = EVP_MD_CTX_new();
    if (sign_ctx == NULL ||
        EVP_DigestSignInit(sign_ctx, NULL, EVP_sha256(), NULL, loaded) != 1 ||
        EVP_DigestSignUpdate(sign_ctx, message, sizeof(message) - 1) != 1 ||
        EVP_DigestSignFinal(sign_ctx, signature, &signature_len) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    /* 7. 复用上下文前 reset，把签名状态清回初始状态。 */
    EVP_MD_CTX_reset(sign_ctx);
    if (EVP_DigestVerifyInit(sign_ctx, NULL, EVP_sha256(), NULL, key) != 1 ||
        EVP_DigestVerifyUpdate(sign_ctx, message, sizeof(message) - 1) != 1 ||
        /* 返回 1 才表示签名验证通过；0 表示不匹配。 */
        EVP_DigestVerifyFinal(sign_ctx, signature, signature_len) != 1) {
        ERR_print_errors_fp(stderr);
        goto done;
    }

    printf("EC key generated, serialized, reloaded and verified (%zu-byte signature)\n",
           signature_len);
    ok = 1;

done:
    /* 每个 *_new/解析/生成对象都在这里对应一次 free。 */
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
