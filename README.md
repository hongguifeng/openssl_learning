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
10. [第 9 章：版本演进与迁移](docs/09-versioning.md)
11. [第 10 章：STM32/FreeRTOS 集成](docs/10-stm32-freertos.md)
12. [第 11 章：调试、测试与性能](docs/11-testing.md)
13. [第 12 章：生产安全清单](docs/12-production.md)

## 代码约定

- 默认使用 OpenSSL 3.x 的高层 EVP/SSL 接口，低级算法接口只在解释历史实现时出现。
- 所有实验都提供成功路径、失败路径和可验证的预期结果。
- 教程中的密钥、口令和自签名证书只用于本地实验，不能直接用于生产设备。
- STM32/FreeRTOS 章节给出移植边界和示例适配层；具体 HAL、网卡驱动和硬件熵源必须由目标项目实现。
