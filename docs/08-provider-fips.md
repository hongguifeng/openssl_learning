# 第 8 章：Provider、FIPS 与源码级调用链

## 8.1 架构

```text
应用
  ↓ EVP / SSL / OSSL_PARAM
libcrypto / libssl（公共 API、对象生命周期、协议状态机）
  ↓ fetch + property query
Core（分配、参数、线程/错误、provider 调度）
  ↓ OSSL_DISPATCH
Provider（算法实现、KEYMGMT、ENCODER/DECODER、RAND 等）
```

OpenSSL 3.x 通过算法名称和属性查询选择实现。`EVP_MD_fetch(NULL, "SHA256", "provider=default")` 会从默认库上下文中查找 provider 提供的 SHA-256；`OSSL_LIB_CTX` 可把配置和算法集合隔离到独立上下文。

## 8.2 自定义 Provider 实验

`labs/08_provider` 提供了可加载的 `learn_provider.so`。其中 `LEARN-TOY-DIGEST` 是故意不安全的玩具摘要，只用于展示：

- `OSSL_provider_init` 如何返回 dispatch 表；
- provider 如何注册算法名称、属性和回调；
- EVP 如何 fetch 算法并回调 `newctx/init/update/final`。

```sh
cmake --build build --target openssl_provider_demo
OPENSSL_MODULES="$PWD/build/labs/08_provider" \
  ./build/labs/08_provider/openssl_provider_demo
```

## 8.3 FIPS

FIPS 不是一个编译宏，也不是把任意自定义 provider 标成“安全”即可。生产使用必须采用目标发行版提供的、经过相应验证的 FIPS 模块、配置和操作流程，并按边界、算法、自检、密钥管理和部署文档执行。本仓库只演示 provider 机制和检查方式，不声称提供 FIPS 验证模块。

可以先检查本机是否安装 FIPS provider：

```sh
openssl list -providers
openssl fipsinstall -help 2>/dev/null || true
```

## 8.4 源码阅读路线

建议在 OpenSSL 源码树中按以下顺序搜索（版本目录名可能不同）：

1. `crypto/evp/evp_fetch.c`：名称/属性查询和 fetch；
2. `crypto/provider_core.c`：provider 加载与 dispatch 连接；
3. `providers/implementations/digests/`：默认 provider 的算法实现；
4. `ssl/statem/`：TLS 握手状态机；
5. `ssl/record/`：记录层读写和加解密；
6. `crypto/params.c` 与 `include/openssl/params.h`：参数传递。

调试时可在应用入口、EVP fetch、provider 回调和 SSL 状态回调分别打印对象地址、算法名、属性和错误栈，形成“应用 → EVP → Core → Provider”的调用证据链。

本仓库还提供了可执行的 GDB 追踪脚本：

```sh
./scripts/trace_provider_call_chain.sh
```

它会在 `EVP_MD_fetch` 和自定义 provider 的 `toy_update` 上设置断点，打印短回溯后继续运行。若目标系统没有调试器，可手动使用同名断点；断点不可用时应先检查是否保留调试符号以及 provider 是否从预期目录加载。
