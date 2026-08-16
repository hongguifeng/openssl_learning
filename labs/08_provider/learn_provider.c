/*
 * 教学用 Provider：LEARN-TOY-DIGEST 不是密码学安全摘要。
 * 它故意只维护一个简单的 32 位状态，用来展示 OpenSSL 3.x 的
 * Core -> Provider dispatch 形状。绝不能用于真实密码学或 FIPS。
 */
#include <openssl/core_dispatch.h> /* OSSL_FUNC_*、OSSL_ALGORITHM */
#include <openssl/core_names.h>    /* OSSL_DIGEST_PARAM_* */
#include <openssl/params.h>        /* OSSL_PARAM_* */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t state; /* 玩具状态，不是安全摘要内部状态 */
    size_t count;   /* 已处理的输入长度 */
} TOY_CTX;

/* EVP_MD_CTX 创建时，Core 会通过 dispatch 调用 provider 的 newctx。 */
static void *toy_newctx(void *provctx) {
    (void)provctx;
    return calloc(1, sizeof(TOY_CTX));
}

static void toy_freectx(void *vctx) {
    free(vctx);
}

/* EVP 复制上下文时需要独立的内存，不能返回原指针。 */
static void *toy_dupctx(void *vctx) {
    TOY_CTX *copy = malloc(sizeof(TOY_CTX));
    if (copy != NULL) {
        memcpy(copy, vctx, sizeof(*copy));
    }
    return copy;
}

/* Init 重置一次摘要操作的状态；params 可用于接收算法参数。 */
static int toy_init(void *vctx, const OSSL_PARAM params[]) {
    TOY_CTX *ctx = vctx;
    (void)params;
    ctx->state = UINT32_C(2166136261);
    ctx->count = 0;
    return 1;
}

/* Update 可能被 EVP 多次调用，必须支持任意分块边界。 */
static int toy_update(void *vctx, const unsigned char *data, size_t datalen) {
    TOY_CTX *ctx = vctx;
    for (size_t i = 0; i < datalen; ++i) {
        ctx->state ^= data[i];
        ctx->state *= UINT32_C(16777619);
    }
    ctx->count += datalen;
    return 1;
}

/* Final 把内部状态编码为固定 32 字节；这里只是教学输出。 */
static int toy_final(void *vctx, unsigned char *out, size_t *outl,
                     size_t outsz) {
    TOY_CTX *ctx = vctx;
    if (outsz < 32) {
        return 0;
    }
    for (size_t i = 0; i < 32; ++i) {
        uint32_t value = ctx->state ^ (uint32_t)(ctx->count + i * 0x9e3779b9U);
        out[i] = (unsigned char)(value >> ((i % 4) * 8));
    }
    *outl = 32;
    return 1;
}

/* Core 询问 provider 支持哪些 digest 参数时会调用这个函数。 */
static const OSSL_PARAM *toy_gettable_params(void *provctx) {
    static const OSSL_PARAM params[] = {
        OSSL_PARAM_size_t(OSSL_DIGEST_PARAM_SIZE, NULL),
        OSSL_PARAM_size_t(OSSL_DIGEST_PARAM_BLOCK_SIZE, NULL),
        OSSL_PARAM_END
    };
    (void)provctx;
    return params;
}

/* Core 真正读取参数值时调用；只填写它询问的字段。 */
static int toy_get_params(OSSL_PARAM params[]) {
    OSSL_PARAM *p = OSSL_PARAM_locate(params, OSSL_DIGEST_PARAM_SIZE);
    if (p != NULL && !OSSL_PARAM_set_size_t(p, 32)) {
        return 0;
    }
    p = OSSL_PARAM_locate(params, OSSL_DIGEST_PARAM_BLOCK_SIZE);
    if (p != NULL && !OSSL_PARAM_set_size_t(p, 64)) {
        return 0;
    }
    return 1;
}

/* 这个表把“操作编号”映射到 provider 函数。 */
static const OSSL_DISPATCH toy_digest_functions[] = {
    { OSSL_FUNC_DIGEST_NEWCTX, (void (*)(void))toy_newctx },
    { OSSL_FUNC_DIGEST_FREECTX, (void (*)(void))toy_freectx },
    { OSSL_FUNC_DIGEST_DUPCTX, (void (*)(void))toy_dupctx },
    { OSSL_FUNC_DIGEST_INIT, (void (*)(void))toy_init },
    { OSSL_FUNC_DIGEST_UPDATE, (void (*)(void))toy_update },
    { OSSL_FUNC_DIGEST_FINAL, (void (*)(void))toy_final },
    { OSSL_FUNC_DIGEST_GET_PARAMS, (void (*)(void))toy_get_params },
    { OSSL_FUNC_DIGEST_GETTABLE_PARAMS, (void (*)(void))toy_gettable_params },
    { 0, NULL }
};

/* name 可以包含别名；property definition 用于 EVP fetch 过滤。 */
static const OSSL_ALGORITHM toy_digests[] = {
    { "LEARN-TOY-DIGEST:LEARN-TOY", "provider=learn", toy_digest_functions,
      "Educational non-cryptographic digest" },
    { NULL, NULL, NULL, NULL }
};

/* Core 按 operation 查询 provider 提供的算法集合。 */
static const OSSL_ALGORITHM *toy_query(void *provctx, int operation,
                                       int *no_cache) {
    (void)provctx;
    *no_cache = 0;
    return operation == OSSL_OP_DIGEST ? toy_digests : NULL;
}

static const OSSL_PARAM *toy_prov_gettable(void *provctx) {
    static const OSSL_PARAM params[] = {
        OSSL_PARAM_utf8_ptr(OSSL_PROV_PARAM_NAME, NULL, 0),
        OSSL_PARAM_utf8_ptr(OSSL_PROV_PARAM_VERSION, NULL, 0),
        OSSL_PARAM_END
    };
    (void)provctx;
    return params;
}

static int toy_prov_get(void *provctx, OSSL_PARAM params[]) {
    OSSL_PARAM *p;
    (void)provctx;
    p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_NAME);
    if (p != NULL && !OSSL_PARAM_set_utf8_ptr(p, "OpenSSL Learning Provider")) {
        return 0;
    }
    p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_VERSION);
    if (p != NULL && !OSSL_PARAM_set_utf8_ptr(p, "0.1")) {
        return 0;
    }
    return 1;
}

static const OSSL_DISPATCH toy_provider_functions[] = {
    { OSSL_FUNC_PROVIDER_GETTABLE_PARAMS, (void (*)(void))toy_prov_gettable },
    { OSSL_FUNC_PROVIDER_GET_PARAMS, (void (*)(void))toy_prov_get },
    { OSSL_FUNC_PROVIDER_QUERY_OPERATION, (void (*)(void))toy_query },
    { 0, NULL }
};

/*
 * 这是 provider 模块的入口。Core 将自己的 dispatch 表通过 in 传入，
 * provider 返回 out；本示例没有使用 Core 服务，所以不读取 in。
 */
int OSSL_provider_init(const OSSL_CORE_HANDLE *handle,
                       const OSSL_DISPATCH *in,
                       const OSSL_DISPATCH **out,
                       void **provctx) {
    (void)handle;
    (void)in;
    *provctx = NULL;
    *out = toy_provider_functions;
    return 1;
}
