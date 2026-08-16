/*
 * 教学用 Provider：LEARN-TOY-DIGEST 不是密码学安全摘要。
 * 它只用于展示 OpenSSL 3.x 的 Provider/Core/Dispatch 接口形状。
 */
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t state;
    size_t count;
} TOY_CTX;

static void *toy_newctx(void *provctx) {
    (void)provctx;
    return calloc(1, sizeof(TOY_CTX));
}

static void toy_freectx(void *vctx) {
    free(vctx);
}

static void *toy_dupctx(void *vctx) {
    TOY_CTX *copy = malloc(sizeof(TOY_CTX));
    if (copy != NULL) {
        memcpy(copy, vctx, sizeof(*copy));
    }
    return copy;
}

static int toy_init(void *vctx, const OSSL_PARAM params[]) {
    TOY_CTX *ctx = vctx;
    (void)params;
    ctx->state = UINT32_C(2166136261);
    ctx->count = 0;
    return 1;
}

static int toy_update(void *vctx, const unsigned char *data, size_t datalen) {
    TOY_CTX *ctx = vctx;
    for (size_t i = 0; i < datalen; ++i) {
        ctx->state ^= data[i];
        ctx->state *= UINT32_C(16777619);
    }
    ctx->count += datalen;
    return 1;
}

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

static const OSSL_PARAM *toy_gettable_params(void *provctx) {
    static const OSSL_PARAM params[] = {
        OSSL_PARAM_size_t(OSSL_DIGEST_PARAM_SIZE, NULL),
        OSSL_PARAM_size_t(OSSL_DIGEST_PARAM_BLOCK_SIZE, NULL),
        OSSL_PARAM_END
    };
    (void)provctx;
    return params;
}

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

static const OSSL_ALGORITHM toy_digests[] = {
    { "LEARN-TOY-DIGEST:LEARN-TOY", "provider=learn", toy_digest_functions,
      "Educational non-cryptographic digest" },
    { NULL, NULL, NULL, NULL }
};

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
