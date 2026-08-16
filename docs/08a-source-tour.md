# 第 8A 章：把 OpenSSL 源码放进构建环境，沿真实调用链读架构

这一章不再只画概念图，而是让 CMake 获取与实验环境匹配的 OpenSSL
3.0.2 源码，并从公共 API 一直追到 EVP fetch、Provider dispatch、TLS
状态机和 record/BIO 层。

## 8A.1 为什么使用 FetchContent，但不使用 MakeAvailable

OpenSSL 上游使用 Perl `Configure` + Make/NMake 构建，不是原生 CMake
工程。因此本教程采用：

```cmake
FetchContent_Declare(openssl_source ...)
FetchContent_Populate(openssl_source)
```

CMake 负责下载、哈希校验、解压和确定源码路径，但不会误把 OpenSSL
当作含 `CMakeLists.txt` 的子项目。应用实验仍链接系统
`OpenSSL::Crypto/OpenSSL::SSL`；源码树用于阅读、索引、断点和后续单独
构建。

## 8A.2 获取源码并生成索引

```sh
cmake -S . -B build-source \
  -DOPENSSL_FETCH_SOURCE=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-source --target openssl_source_show
cmake --build build-source --target openssl_source_index
```

源码默认位于 `build-source/_deps/openssl_source-src`，生成报告位于
`build-source/openssl-source-index.md`。归档固定为 `openssl-3.0.2`，并通过
SHA-256 校验，保证源码与本教程的验证基线一致。

离线或已经有源码时：

```sh
cmake -S . -B build-source \
  -DOPENSSL_FETCH_SOURCE=ON \
  -DOPENSSL_SOURCE_OVERRIDE=/path/to/openssl-3.0.2
```

## 8A.3 先认识源码根目录

| 目录 | 你应该把它理解成什么 |
| --- | --- |
| `include/openssl` | 安装给应用使用的公共 API；稳定性承诺主要在这里 |
| `include/internal` | OpenSSL 内部跨模块接口；应用不能依赖 |
| `crypto` | libcrypto：EVP、X.509、ASN.1、BIO、RAND、Provider Core |
| `providers` | 算法/provider 实现与入口 |
| `ssl` | libssl：TLS 公共 API、状态机、record、session |
| `apps` | `openssl` CLI；也是很有价值的公共 API 用例库 |
| `test` | 回归测试和 recipe，能解释边界行为 |
| `util` | 符号清单、生成脚本和平台构建辅助 |

接口“在头文件里声明”不代表实现就在同名 C 文件。OpenSSL 经常通过宏、
生成的 dispatch、函数指针和 Provider 表连接模块，所以需要结合 `rg`、
GDB 和生成索引阅读。

## 8A.4 OpenSSL 公共接口的设计规则

### Opaque 类型

应用看到 `EVP_MD_CTX *`、`SSL *`、`X509 *`，但看不到结构体字段。
OpenSSL 因此可以改变内部布局，而不要求应用改写字段访问。代价是所有状态
都必须通过 getter/setter 或操作函数访问。

### `new/free`、引用计数和所有权命名

- `*_new` 创建对象，通常由调用方 `*_free`；
- `*_up_ref` 增加引用计数；
- `get0_*` 通常返回借用指针，调用方不能释放；
- `get1_*` 通常增加引用，调用方需要释放；
- `set0_*` 通常转移所有权；
- `set1_*` 通常复制或增加引用。

这些是强约定，但仍需查具体 API 文档。教程中的 `SSL_set_bio` 就是所有权
转移的典型例子。

### Context + Init/Update/Final

摘要、加密、签名和 TLS 都采用“长期配置/一次操作状态”分离：

```text
算法或配置对象 → 创建操作上下文 → Init → Update/状态推进 → Final → free
```

这允许流式输入、非阻塞状态机、Provider 替换和长期配置复用。

### 错误队列

多数函数只返回简短状态，把详细原因压入线程局部错误队列。模块通过统一
错误设施报告原因，应用必须在原线程及时读取。

## 8A.5 从 `EVP_MD_fetch` 追到 Provider

按以下顺序阅读生成索引中的位置：

```text
include/openssl/evp.h
  ↓ 公共声明
crypto/evp/digest.c: EVP_MD_fetch
  ↓ 通用 fetch 包装
crypto/evp/evp_fetch.c: evp_generic_fetch / inner_evp_generic_fetch
  ↓ method store、名称、属性和缓存
crypto/core_fetch.c: ossl_method_construct
  ↓ provider query_operation
providers/defltprov.c + implementations/digests
  ↓ OSSL_DISPATCH 返回 init/update/final
```

调用 `EVP_DigestUpdate` 时，EVP 上下文已经保存 Provider dispatch 函数
指针，所以应用不需要知道 SHA-256 来自 default、FIPS 还是硬件 Provider。

## 8A.6 从 `SSL_do_handshake` 追到 BIO

```text
include/openssl/ssl.h: SSL_do_handshake
  ↓
ssl/ssl_lib.c: 公共入口，调用 SSL 对象中的 handshake_func
  ↓
ssl/statem/statem.c: ossl_statem_connect / state_machine
  ↓
ssl/statem/statem_clnt.c 或 statem_srvr.c: 构造/处理具体握手消息
  ↓
ssl/record: TLS record 封装、加解密、序号和读取
  ↓
BIO: socket 或内存 BIO pair
```

这解释了为什么 `SSL_do_handshake` 可能返回 WANT_READ/WANT_WRITE：状态机
已经推进到需要读取或写出 record 的位置，但底层 BIO 暂时不能完成。

## 8A.7 编译生成物和 API 稳定性

OpenSSL 的部分头文件、错误码和符号版本由构建脚本生成。
`util/libcrypto.num`、`util/libssl.num` 等文件参与导出符号和 ABI 管理。
阅读源码时要区分：

- 仓库中的源模板；
- `Configure` 后生成的头文件；
- 安装目录中的最终公共头文件；
- Provider 模块的动态 dispatch ABI。

不要从 `include/internal` 复制结构体到应用。它们可以在版本演进中变化，
不属于应用 ABI。

## 8A.8 实验路线

1. 构建 `openssl_source_index`，逐项打开报告中的路径；
2. 对照 `labs/03_evp`，从 `EVP_MD_fetch` 追到 Provider；
3. 对照 `labs/07_tls`，从 `SSL_do_handshake` 追到状态机和 record；
4. 运行 `scripts/trace_provider_call_chain.sh`，把源码路径与真实调用栈对应；
5. 在源码的 `test/recipes` 中搜索同一个 API，观察上游如何测试失败路径。

完成本章后，你应该能回答：一个公共 API 在哪一层验证参数、哪一层保存
状态、哪一层选择实现、哪一层接触底层 I/O。
