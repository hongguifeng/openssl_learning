# 第 8C 章：libcrypto 子系统地图——BIO、X.509、RAND、编码器如何协作

OpenSSL 不只有 EVP 和 TLS。一个真实 TLS 连接同时依赖 BIO、RAND、
X.509、ASN.1、编码器/解码器、对象数据库、属性查询和 Provider。本章按
“输入从哪里来、对象怎样形成、最后被谁消费”串起这些模块。

## 8C.1 总体依赖图

```text
PEM/DER 文件或内存
  ↓ BIO
PEM + ASN.1 + OSSL_DECODER
  ↓
EVP_PKEY / X509
  ↓                 RAND → EVP_RAND → Provider DRBG → 系统/硬件熵
X509_STORE_CTX       │
  ↓                  │
证书链验证            │
  └──────────────┬────┘
                 ↓
           libssl TLS 状态机
                 ↓
       EVP cipher/digest/KDF/signature
                 ↓
             Provider 实现
                 ↓
           record 层 → BIO → socket
```

图中每个箭头都跨越一个对象边界。调试时应该先判断问题在哪个对象形成阶段，
而不是把所有错误都归为“TLS 失败”。

## 8C.2 BIO：统一 I/O 方法表

公共头文件只暴露 `BIO *` 和 `BIO_METHOD *`。内部实现位于：

```text
include/internal/bio.h : struct bio_method_st
crypto/bio/bio_local.h : struct bio_st
crypto/bio/bio_lib.c   : BIO_read_ex / BIO_write_ex 公共包装
```

`BIO_METHOD` 是一组 create/destroy/read/write/ctrl 回调；`BIO` 保存当前方法、
私有数据、状态、引用计数以及 `next_bio/prev_bio`。过滤 BIO 通过链表把调用
交给下一个 BIO：

```text
BIO_write(filter)
  → filter method write_ex
  → 转换数据
  → BIO_write_ex(BIO_next(filter))
```

这解释了两个公共接口规则：

- BIO 是对象，不是文件描述符；自定义 BIO 必须维护 retry flags 和 ctrl 语义；
- BIO 链包含所有权，不能只释放中间节点或重复释放底层节点。

Provider 需要读写密钥材料时不能直接依赖应用 BIO 结构，因此 Core 还通过
`OSSL_CORE_BIO` dispatch 提供受控 I/O upcall。

## 8C.3 PEM、ASN.1、Decoder/Encoder 的分工

```text
PEM：文本包裹和口令处理
ASN.1：DER 结构的编码/解码规则
OSSL_DECODER：按输入格式/结构/密钥类型寻找 Provider 解码器
OSSL_ENCODER：把 Provider 密钥对象输出为 DER/PEM/TEXT
```

以 `PEM_read_bio_PrivateKey()` 为例，3.x 内部会创建
`OSSL_DECODER_CTX_new_for_pkey()`。Decoder 方法也通过 method construction 和
Provider fetch 发现，而不是在 PEM 层硬编码所有密钥算法。

大致流程：

```text
BIO 中的 PEM
  → 去掉 PEM armor、处理口令
  → Decoder 链识别 DER 数据结构
  → Provider decoder 解析算法参数/密钥材料
  → Provider keymgmt 创建内部 keydata
  → libcrypto 包装为 EVP_PKEY
```

反向输出由 `OSSL_ENCODER_CTX_new_for_pkey()` 完成。Keymgmt、encoder 和
signature 必须对同一个 Provider 密钥对象达成一致，这也是硬件 Provider 不只
实现一个 `sign()` 回调就能完整工作的原因。

## 8C.4 X.509 验证内部阶段

证书解析主要依赖 ASN.1；证书验证核心位于 `crypto/x509/x509_vfy.c`。
实际 `X509_verify_cert()` 会选择普通链验证或 DANE 路径。普通路径
`verify_chain()` 不是一次签名检查，而是一串策略阶段：

```text
build_chain(ctx)                构建候选链，寻找 trust anchor
  ↓
check_extensions/check_auth_level
  ↓
check_id(ctx)                   DNS/email/IP 身份匹配
  ↓
check_revocation(ctx)           配置启用时处理 CRL 等
  ↓
internal_verify(ctx)            验证签名、时间和链关系
```

`X509_STORE_CTX` 保存目标证书、未受信中间证书、信任库、验证参数、当前错误、
错误深度和构造出的链。验证回调可以观察或改变部分行为，但错误地“回调总是
返回 1”会绕过安全策略。

主机名检查最终进入 `check_id()`，所以“链签名正确”和“主机名正确”是两个
阶段。教程的错误 SAN 实验正是在验证这条路径。

## 8C.5 RAND：`RAND_bytes` 背后不只是读一次硬件 RNG

公共入口 `RAND_bytes_ex()` 位于 `crypto/rand/rand_lib.c`。OpenSSL 3.x 会为
库上下文取得随机数生成器上下文，并通过 `EVP_RAND_fetch()` 获取实现。

```text
RAND_bytes_ex(libctx, out, len, strength)
  ↓
库上下文的 public/private DRBG
  ↓ EVP_RAND generate
Provider CTR-DRBG/HASH-DRBG/HMAC-DRBG
  ↓ reseed/instantiate 所需熵
seed source：OS entropy、硬件或父 RAND
```

Provider DRBG 实现位于 `providers/implementations/rands`。它维护实例化状态、
安全强度、重播/重新播种计数、prediction resistance 和父子 RAND 关系。

对 STM32 的启示是：硬件 TRNG 是熵源，不一定等同于应用每次直接读取的
`RAND_bytes` 实现。正确集成需要把硬件熵接入 OpenSSL 期望的 seed/RAND
边界，并让熵源故障传播到 DRBG 实例化或生成失败。

## 8C.6 属性、名称和库上下文是横切基础设施

EVP、RAND、Decoder、Encoder、Keymgmt 都要解决相同问题：

```text
在某个 OSSL_LIB_CTX 中，按 operation + name + property 找到方法
```

因此 `crypto/core_fetch.c`、name map、property parser 和 method store 是横切
基础设施。它们不属于某个具体算法，却决定了所有 Provider 方法的发现、缓存
和选择。

库上下文还容纳 Provider store、RAND 状态、默认属性和配置。把 `NULL`
libctx 传给 API 等于选择默认上下文，不等于“没有上下文”。

## 8C.7 错误模块如何贯穿所有层

每一层只返回简短状态，详细错误压入线程局部错误队列：

```text
Provider 回调失败
  → Provider/Core 错误原因
  → EVP/Decoder/SSL 包装层追加上下文
  → 应用在同一线程读取 ERR 队列
```

读取太晚会混入后续错误；只保留最后一个整数又会丢掉原因链。嵌入式产品应
把错误映射为稳定业务故障码，同时在安全日志中保留受控的 OpenSSL 原因信息。

## 8C.8 源码实验

重新生成索引后，报告会增加 BIO、X.509、RAND 和编解码路径：

```sh
cmake --build build-source --target openssl_source_index
cmake --build build-source --target openssl_source_verify
```

选择一个场景做端到端追踪：

1. 私钥读取：`PEM_read_bio_PrivateKey → OSSL_DECODER → keymgmt → EVP_PKEY`；
2. 证书验证：`X509_verify_cert → build_chain → check_id → internal_verify`；
3. 随机数：`RAND_bytes_ex → EVP_RAND → Provider DRBG → seed source`；
4. TLS：`SSL_do_handshake → state machine → EVP/RAND/X.509 → record → BIO`。

每次只追一条链，并记录对象所有权、失败返回值和跨模块边界。这样比从
`ssl/` 目录第一行顺序读到最后一行有效得多。
