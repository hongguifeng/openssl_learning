# OpenSSL 3.x 嵌入式开发教程

这是一个面向嵌入式软件工程师的 OpenSSL 学习与实战仓库，主线为：

- C 语言
- OpenSSL 3.x（本机验证基线：OpenSSL 3.0.2）
- CMake + Ninja/Make
- STM32 + FreeRTOS 的移植与工程化思路

教程会从 OpenSSL CLI 和最小 C 程序开始，逐步进入 EVP、BIO、X.509、TLS 1.3、Provider/FIPS、源码调用链、交叉编译、资源裁剪和安全测试。实验优先使用本地文件和本地回环连接，不依赖 Docker/Podman。

## 快速开始

```sh
./scripts/check_env.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

也可以用一个命令执行环境检查、构建、现代 API 扫描和全部 CTest：

```sh
./scripts/verify_tutorial.sh
```

## OpenSSL 源码探索

普通构建链接系统 OpenSSL，不会下载源码。需要学习内部模块时，启用独立的
FetchContent 构建目录：

```sh
cmake -S . -B build-source -DOPENSSL_FETCH_SOURCE=ON
cmake --build build-source --target openssl_source_index
cmake --build build-source --target openssl_source_verify
ctest --test-dir build-source -R openssl_source_structure --output-on-failure
```

源码会经过固定 SHA-256 校验，随后生成
`build-source/openssl-source-index.md`。如果已经有 OpenSSL 3.0.2 源码，可用
`-DOPENSSL_SOURCE_OVERRIDE=/path/to/source` 进行离线探索。

如果系统没有 Ninja，CMake 会自动使用可用的 Make 后端。OpenSSL 开发头文件通常由发行版的 `libssl-dev`/对应开发包提供。

## 学习顺序

1. [第 0 章：环境与密码学边界](docs/00-environment.md)
2. [第 1 章：OpenSSL CLI](docs/01-cli.md)
3. [第 2 章：CMake、链接与错误处理](docs/02-cmake-c-api.md)
4. [第 3 章：EVP 摘要与 AEAD](docs/03-evp.md)
5. [第 4 章：密钥与序列化](docs/04-keys.md)
6. [第 5 章：BIO 与数据流](docs/05-bio.md)
7. [第 6 章：X.509 与 PKI](docs/06-x509.md)
8. [第 7 章：TLS 1.3](docs/07-tls.md)
9. [第 8 章：Provider、FIPS 与源码调用链](docs/08-provider-fips.md)
10. [第 8A 章：FetchContent 源码导览与接口设计](docs/08a-source-tour.md)
11. [第 8B 章：对象内部结构与模块协作](docs/08b-internals-deep-dive.md)
12. [第 8C 章：BIO、X.509、RAND 与编解码模块](docs/08c-subsystem-map.md)
13. [第 9 章：版本演进与迁移](docs/09-versioning.md)
14. [第 10 章：STM32/FreeRTOS 集成](docs/10-stm32-freertos.md)
15. [第 11 章：调试、测试与性能](docs/11-testing.md)
16. [第 12 章：生产安全清单](docs/12-production.md)


## 推荐学习方法

不要只运行命令然后跳到下一章。每一章按下面的顺序学习：

1. 先读“问题是什么”和“为什么这样设计”；
2. 画出对象、输入、输出和所有权；
3. 先运行成功用例，再运行失败用例；
4. 对照 CLI、C 代码和错误输出；
5. 修改一个参数，预测结果后再运行；
6. 把结论写成自己的检查清单。

如果某个 API 看不懂，先回到本章的对象生命周期图，不要直接记函数名。OpenSSL 代码的难点通常不是 C 语法，而是“哪个对象保存状态、哪个对象拥有内存、失败后谁负责清理”。

## 每个阶段的完成标准

- 第 0～2 章：能解释 `libcrypto`/`libssl`、编译/链接/运行时依赖和错误队列；
- 第 3～4 章：能用 EVP 完成摘要、AEAD、签名，并说明每个上下文的生命周期；
- 第 5～7 章：能解释 BIO、证书验证和 TLS 握手的字节流/状态机；
- 第 8～9 章：能追踪一次 fetch 到 provider 回调，并能识别旧 API 迁移点；
- 第 10～12 章：能为 STM32/FreeRTOS 写出 RNG、时间、内存、锁和网络适配验收计划。

## 先运行，再阅读源码

```sh
./scripts/verify_tutorial.sh
./scripts/trace_provider_call_chain.sh
```

前一个命令确认环境和实验基线，后一个命令把架构章节中的“应用 → EVP → Provider”变成真实调用栈。实验输出可能因 OpenSSL 小版本、编译器和路径不同而变化，但成功/失败条件应保持一致。

## 代码约定

- 默认使用 OpenSSL 3.x 的高层 EVP/SSL 接口，低级算法接口只在解释历史实现时出现。
- 所有实验都提供成功路径、失败路径和可验证的预期结果。
- 教程中的密钥、口令和自签名证书只用于本地实验，不能直接用于生产设备。
- STM32/FreeRTOS 章节给出移植边界和示例适配层；具体 HAL、网卡驱动和硬件熵源必须由目标项目实现。
