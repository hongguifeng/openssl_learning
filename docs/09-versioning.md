# 第 9 章：版本演进、API 迁移与兼容性设计

版本迁移不是把头文件路径改一下。OpenSSL 1.0.2、1.1.1 和 3.x 在结构体可见性、初始化、TLS、算法发现和扩展模块方面都有不同假设。

## 9.1 时间线与设计变化

| 版本 | 设计特征 | 对应用的影响 |
| --- | --- | --- |
| 1.0.2 | 许多结构体公开；ENGINE 常见 | 应用可能直接访问内部字段；线程初始化更依赖应用 |
| 1.1.0 | 结构体 opaque；库自动初始化 | 旧代码的结构体栈分配和字段访问会失败 |
| 1.1.1 | TLS 1.3 | 握手消息、密码套件和 API 行为增加新分支 |
| 3.0+ | Provider、fetch、参数 API、FIPS provider | 低级算法接口逐步弃用，应用应迁移到 EVP |

## 9.2 典型迁移

旧式代码常见：

```c
SHA256_CTX ctx;
SHA256_Init(&ctx);
SHA256_Update(&ctx, data, len);
SHA256_Final(out, &ctx);
```

现代主线：

```c
EVP_MD_CTX *ctx = EVP_MD_CTX_new();
EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
EVP_DigestUpdate(ctx, data, len);
EVP_DigestFinal_ex(ctx, out, &out_len);
EVP_MD_CTX_free(ctx);
```

迁移收益不只是“消除弃用警告”：EVP 代码可以使用 provider 实现、属性查询、FIPS 边界和统一的错误/生命周期模式。

## 9.3 API、ABI 和运行时配置是三个维度

- **API**：头文件中函数和类型是否存在；
- **ABI**：已编译程序能否链接到目标库；
- **运行时配置**：provider 模块、配置文件、模块搜索路径是否正确。

嵌入式发布时，不能只把 `libcrypto.a` 拷进固件就结束；还要确认 provider 是否静态内置、是否需要配置文件、是否依赖文件系统和动态加载器。

## 9.4 迁移方法

1. 先在旧版本上建立行为测试：摘要向量、签名验签、证书错误和 TLS 互操作；
2. 打开弃用警告，禁止新增低级 API；
3. 按功能迁移到 EVP，不要一次性把所有模块重写；
4. 检查 provider、配置和算法属性；
5. 在每个支持版本上构建并运行同一测试集；
6. 记录行为变化，而不是只记录“编译通过”。

## 9.5 实验：现代 API 扫描

```sh
./scripts/check_modern_api.sh
```

脚本扫描实验代码中的典型 `ENGINE_`、低级摘要、AES 和 RSA 符号。它不是完整静态分析，但适合作为教学门禁。真实工程还要使用编译器弃用警告、clang-tidy、SAST 和跨版本 CI。

## 9.6 学习练习

- 把一个低级 SHA-256 函数迁移到 EVP，并为输入分块写测试；
- 分别在 OpenSSL 3.0 和较新 3.x 上运行同一测试，记录输出和错误差异；
- 写一份“支持 OpenSSL 版本矩阵”，包含库、provider、编译器和目标架构；
- 解释为什么 `OPENSSL_VERSION_NUMBER` 不应该成为所有行为分支的唯一依据。
