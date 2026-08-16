# 第 8B 章：对象内部结构与模块协作——从源码理解接口为什么这样设计

第 8A 章回答“函数在哪里”；本章回答“为什么公共接口要设计成现在这样”。
所有路径都来自 CMake 获取并验证过的 OpenSSL 3.0.2 源码。

## 8B.1 公共声明和内部定义为什么分开

应用在 `include/openssl/types.h` 中看到的只是：

```c
typedef struct evp_md_st EVP_MD;
typedef struct evp_md_ctx_st EVP_MD_CTX;
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ossl_provider_st OSSL_PROVIDER;
```

这些是不完整类型。应用可以保存指针，却不能计算 `sizeof(SSL)` 或访问字段。
真实定义分别位于：

| 对象 | 内部定义位置 |
| --- | --- |
| `EVP_MD` | `include/crypto/evp.h` |
| `EVP_MD_CTX` | `crypto/evp/evp_local.h` |
| `OSSL_PROVIDER` | `crypto/provider_core.c` |
| `SSL_CTX`、`SSL` | `ssl/ssl_local.h` |

这样做带来两个直接结果：

1. 应用只能通过公共函数维护对象不变量；
2. OpenSSL 可以扩展内部状态，而不让应用依赖字段偏移。

这就是 1.0.2 到 1.1.x 迁移时“结构体变 opaque”的核心设计变化。

## 8B.2 `EVP_MD`：算法描述对象实际上是一张方法表

源码中的 `struct evp_md_st` 同时包含三类信息：

```text
算法身份：name_id、type_name、description
实现来源：OSSL_PROVIDER *prov、引用计数和锁
调用入口：newctx、dinit、dupdate、dfinal、freectx、参数函数
```

OpenSSL 3.x 仍保留部分 legacy 字段，如旧式 `init/update/final` 函数指针。
新的 Provider 路径则保存 `OSSL_FUNC_digest_*` dispatch 指针。

因此 `EVP_MD_fetch()` 的输出不是“一段 SHA-256 代码”，而是：

```text
名称/属性匹配结果
  + Provider 所有权
  + 一组经过 Core 验证并绑定的 digest 回调
  + 引用计数和元数据
```

这解释了为什么 fetched `EVP_MD *` 需要 `EVP_MD_free()`：它是动态构造并
引用 Provider 的方法对象，不等同于静态常量。

## 8B.3 `EVP_MD_CTX`：一次操作如何连接 EVP 和 Provider

`struct evp_md_ctx_st` 中最关键的字段是：

| 字段 | 作用 |
| --- | --- |
| `reqdigest` | 应用最初请求的摘要描述 |
| `digest` | 当前实际使用的方法对象 |
| `pctx` | 签名/验签时关联的 `EVP_PKEY_CTX` |
| `update` | 当前 Update 包装函数 |
| `algctx` | Provider `newctx` 返回的私有状态 |
| `fetched_digest` | 上下文拥有的动态 fetch 方法引用 |

一次调用可以画成：

```text
EVP_MD_CTX
├─ digest/fetched_digest ──> EVP_MD ──> OSSL_PROVIDER
├─ algctx -----------------> Provider 私有摘要状态
└─ update -----------------> EVP 包装/Provider dispatch
```

Provider 私有状态通过 `void *algctx` 隔离。libcrypto 不需要知道 SHA-256
状态结构、硬件会话句柄或 FIPS 模块内部布局；它只按 dispatch 调用。

## 8B.4 Fetch 内部不是每次都扫描所有 Provider

`EVP_MD_fetch()` 在 `crypto/evp/digest.c` 中只是一个很薄的包装：它把
`OSSL_OP_DIGEST`、算法名和属性交给 `evp_generic_fetch()`。

`inner_evp_generic_fetch()` 的主要步骤是：

1. 从 `OSSL_LIB_CTX` 取得 method store 和 name map；
2. 把算法名称映射为 `name_id`；
3. 把 `name_id + operation_id` 组合成 method id；
4. 先查属性相关的 method cache；
5. 未命中时调用 `ossl_method_construct()`；
6. 让可用 Provider 的 `query_operation` 返回 `OSSL_ALGORITHM`；
7. 由 `evp_md_from_algorithm` 验证 dispatch 并构造 `EVP_MD`；
8. 把方法放入 store/cache，返回带引用的对象。

因此 fetch 的性能和结果都与 `OSSL_LIB_CTX`、Provider 激活状态、名称映射、
属性查询和缓存有关。不要在每个数据块的热路径中重复 fetch；通常在模块初始化
时 fetch 方法，然后复用方法对象创建操作上下文。

## 8B.5 `OSSL_PROVIDER`：Core 一侧保存什么

`struct ossl_provider_st` 位于 `crypto/provider_core.c`，关键字段包括：

```text
生命周期：initialized/activated、refcnt、activatecnt、锁
模块加载：name、path、DSO *module、init_function
作用域：OSSL_LIB_CTX *libctx、provider store
Provider 回调：teardown、get_params、self_test、query_operation
跨边界数据：provctx、dispatch
```

动态模块加载后，Core 查找导出符号 `OSSL_provider_init`。入口返回 Provider
dispatch 表和 `provctx`。Core 再从表中提取 `query_operation` 等函数指针。

```text
DSO_load(module)
  → DSO_bind_func("OSSL_provider_init")
  → provider init(handle, core_dispatch, &provider_dispatch, &provctx)
  → Core 保存 query_operation/get_params/teardown
```

Provider 和算法方法都带引用计数/锁，说明“加载成功”不是简单的全局布尔值，
而是受库上下文、激活计数和方法引用影响的生命周期。

## 8B.6 `SSL_CTX` 为什么适合共享，而 `SSL` 不适合

`SSL_CTX` 内部保存：

- `OSSL_LIB_CTX` 和 `SSL_METHOD`；
- TLS 1.2/1.3 密码套件列表；
- `X509_STORE` 信任库；
- 会话缓存、超时和统计；
- 证书、密码、验证和会话回调；
- 引用计数和同步状态。

这些是一组连接共同使用的配置，所以 `SSL_new(ctx)` 会从它创建具体连接。

`SSL` 内部保存：

- `rbio/wbio` 和当前 `rwstate`；
- `handshake_func` 以及客户端/服务器角色；
- `OSSL_STATEM statem` 当前握手状态；
- 当前协议版本、shutdown、session；
- 当前握手消息、record 缓冲、临时密钥和 Finished 摘要。

这些状态都属于单条连接。两个任务同时驱动同一个 `SSL` 会破坏状态机和 BIO
顺序；应用必须保证单连接串行推进，或在更高层严格同步。

## 8B.7 `SSL_do_handshake` 为什么能同时服务客户端和服务器

源码显示 `SSL_do_handshake()` 本身并不包含 ClientHello/ServerHello 逻辑：

1. `SSL_set_connect_state()` 把 `handshake_func` 设置为
   `s->method->ssl_connect`；
2. `SSL_set_accept_state()` 设置为 `ssl_accept`；
3. `SSL_do_handshake()` 检查状态后调用 `s->handshake_func(s)`；
4. 客户端最终进入 `ossl_statem_connect()`，服务器进入
   `ossl_statem_accept()`；
5. 二者都调用 `state_machine(s, server)`，通过角色选择消息处理表。

`state_machine()` 的注释明确给出了顶层流转：

```text
MSG_FLOW_UNINITED
       ↓
MSG_FLOW_WRITING ↔ MSG_FLOW_READING
       ↓
MSG_FLOW_FINISHED
```

发生非阻塞 I/O 时函数可以中途退出，下次调用从保存的子状态继续。这正是
`SSL_ERROR_WANT_READ/WANT_WRITE` 必须被当作“稍后继续”的源码依据。

## 8B.8 Record 层和 BIO 的边界

握手状态机产生/消费的是 handshake message；record 层负责把消息封装为 TLS
record、维护序号并调用密码实现；BIO 负责实际字节运输。

```text
handshake/application state
  ↓
record framing + AEAD
  ↓
SSL 的 rbio/wbio
  ↓
socket、memory BIO 或自定义 BIO
```

`ssl3_read_bytes()` 位于 `ssl/record/rec_layer_s3.c`，它既要处理所需 record
类型，也可能因为握手消息、告警、关闭或非阻塞 BIO 改变控制流。因此应用
不能把 `SSL_read` 当作普通 `recv` 的一对一包装。

## 8B.9 源码实验

```sh
cmake --build build-source --target openssl_source_verify
ctest --test-dir build-source -R openssl_source_structure --output-on-failure
```

验证脚本会确认版本以及 EVP fetch、Provider、TLS 状态机和 record 层的关键
符号位置。然后打开以下文件，把本章图中的每条箭头对应到结构体字段或函数：

```text
include/crypto/evp.h
crypto/evp/evp_local.h
crypto/evp/digest.c
crypto/evp/evp_fetch.c
crypto/provider_core.c
ssl/ssl_local.h
ssl/ssl_lib.c
ssl/statem/statem.c
ssl/record/rec_layer_s3.c
```

不要把内部字段用于应用开发；阅读它们是为了理解公共接口和生命周期，而不是
绕过公共 API。
