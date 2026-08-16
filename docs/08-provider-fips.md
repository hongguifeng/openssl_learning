# 第 8 章：Provider、FIPS 与源码级调用链

这一章回答“EVP 调用的算法到底在哪里实现”。如果只会调用 `EVP_DigestInit_ex`，却不知道算法如何被找到、参数如何传给 provider、模块如何加载，那么遇到自定义硬件加速或 FIPS 配置时仍然无法开发。

## 8.1 为什么 OpenSSL 3.x 要引入 Provider

旧式 OpenSSL 把很多算法实现直接编译进 `libcrypto`，应用或 ENGINE 可能依赖内部结构体。这样做的问题是：

- 算法实现和核心对象生命周期耦合；
- 第三方硬件加速接口难以统一；
- FIPS 边界难以清晰划分；
- 内部结构体变化容易破坏应用 ABI。

OpenSSL 3.x 把“核心服务”和“算法实现”分开：核心负责对象、参数、错误、线程和算法发现；provider 负责摘要、密码、签名、密钥管理、编码器/解码器、随机数等实现。

## 8.2 完整架构图

```text
┌──────────────────────────────────────────────────────────┐
│ 应用：设备协议、文件加密、TLS 客户端                    │
└───────────────┬───────────────────────┬──────────────────┘
                │ EVP / SSL API         │ 配置、OSSL_PARAM
┌───────────────▼───────────────────────▼──────────────────┐
│ libssl / libcrypto                                        │
│ SSL 状态机、EVP 对象、错误队列、OSSL_LIB_CTX、fetch       │
└───────────────┬──────────────────────────────────────────┘
                │ provider dispatch table
┌───────────────▼──────────────────────────────────────────┐
│ Core（由 libcrypto 提供）                                  │
│ 内存/日志/参数/线程服务；加载模块；连接 provider 回调      │
└───────────────┬──────────────────────────────────────────┘
                │ OSSL_DISPATCH + OSSL_PARAM
┌───────────────▼──────────────────────────────────────────┐
│ Provider                                                   │
│ default / legacy / fips / 自定义硬件 provider              │
│ 算法实现、keymgmt、encoder/decoder、RAND 等               │
└──────────────────────────────────────────────────────────┘
```

应用不应该直接调用 provider 的静态函数。正确方向是：应用 → EVP → fetch → provider dispatch。

## 8.3 Fetch：算法名称不是实现指针

`EVP_sha256()` 是方便的默认算法入口；更通用的 OpenSSL 3.x 写法是：

```c
EVP_MD *md = EVP_MD_fetch(libctx, "SHA256", "provider=default");
/* 使用 md ... */
EVP_MD_free(md);
```

fetch 输入三项信息：

1. **库上下文**：`NULL` 表示默认 `OSSL_LIB_CTX`；
2. **算法名称**：可以包含别名；
3. **属性查询**：例如 `provider=default`、`fips=yes`。

属性不是普通字符串标签，而是算法选择约束。查询 `fips=yes` 时，只有声明该属性且完成相应验证边界的实现才应该被选中。

## 8.4 `OSSL_PARAM`：跨边界传递类型化参数

Provider API 不依赖应用和模块共享私有结构体，而是使用参数数组：

```text
OSSL_PARAM[] = {
  { "digest", UTF8_STRING, "SHA256" },
  { "key",    OCTET_STRING, bytes },
  { "size",   SIZE_T,       32 },
  { END }
}
```

这带来两个工程要求：

- 参数名拼写、类型和长度必须匹配；
- 指针指向的内存生命周期必须覆盖调用；
- 不要把 `OSSL_PARAM` 当成可以随意跨线程保存的普通结构体。

## 8.5 `OSSL_LIB_CTX`：隔离配置和算法集合

默认上下文适合简单程序；设备上同时存在“普通 TLS”和“FIPS/硬件会话”时，可以创建独立上下文：

```c
OSSL_LIB_CTX *ctx = OSSL_LIB_CTX_new();
OSSL_PROVIDER_load(ctx, "default");
EVP_MD *md = EVP_MD_fetch(ctx, "SHA256", NULL);
/* ... */
EVP_MD_free(md);
OSSL_LIB_CTX_free(ctx);
```

上下文会影响 provider、配置文件、属性默认值和 fetch 缓存。不要把一个上下文取得的对象随意假设为另一个上下文可用；在接口设计中明确“对象属于哪个上下文”。

## 8.6 自定义 Provider 的生命周期

本仓库的 `learn_provider.c` 是一个非密码学安全的玩具摘要，故意把实现简化为 FNV 风格状态。它展示真实 provider 的结构：

```text
OSSL_provider_init
  └─ 返回 provider dispatch 表
       ├─ GETTABLE_PARAMS / GET_PARAMS：模块信息
       └─ QUERY_OPERATION：告诉 Core 本模块提供 DIGEST
            └─ OSSL_ALGORITHM
                 └─ digest dispatch
                      ├─ NEWCTX / FREECTX
                      ├─ INIT / UPDATE / FINAL
                      └─ GET_PARAMS
```

这不是一个安全摘要，也不能用于产品密码学。真正的硬件 provider 通常还要实现 keymgmt、signature、key exchange、encoder/decoder、参数校验、硬件会话句柄和错误映射。

## 8.7 FIPS 到底是什么

FIPS 不是“把 provider 名称改成 fips”，也不是自定义 provider 加一个属性。生产 FIPS 有明确的模块边界、算法集合、自检、安装配置、操作状态和验证文档。应用开发者需要：

1. 选择目标系统支持的已验证模块版本；
2. 按该模块文档生成配置和安装状态；
3. 通过属性查询/默认属性确保只选中合规实现；
4. 记录启动自检、错误状态和密钥管理边界；
5. 不把普通 default provider 与 FIPS provider 的混用当成合规方案。

本仓库只验证 provider 机制，不声称包含 FIPS 验证模块。主机检查：

```sh
openssl list -providers
openssl fipsinstall -help 2>/dev/null || true
```

## 8.8 源码级调用链实验

```sh
cmake --build build --target openssl_provider_demo
./scripts/trace_provider_call_chain.sh
```

脚本用 GDB 在 `EVP_MD_fetch` 和自定义 provider 的 `toy_update` 设置断点。你应能看到类似：

```text
TRACE: EVP_MD_fetch
#0 EVP_MD_fetch
#1 main ... provider_demo.c
TRACE: provider toy_update
#0 toy_update ... learn_provider.c
#1 main ... provider_demo.c
```

这条证据说明：应用的 fetch 先进入 libcrypto，再经 dispatch 进入 provider。下一步可在源码树中对照：

- `crypto/evp/evp_fetch.c`：fetch、名称和属性；
- `crypto/provider_core.c`：模块加载和 dispatch 连接；
- `providers/implementations/digests/`：默认算法实现；
- `crypto/params.c`：参数访问；
- `ssl/statem/`：TLS 握手状态机。

## 8.9 学习练习

1. 把 demo 的算法查询改成 `EVP_MD_fetch(NULL, "LEARN-TOY-DIGEST", "provider=learn")`，打印 provider 名称；
2. 在 provider 的 `toy_update` 中打印输入长度，观察 EVP Update 的分块行为；
3. 加载 `legacy` provider，比较 fetch 一个旧算法时的属性和来源；
4. 设计一个硬件签名 provider 需要的最小 key handle 生命周期。
