# 第 2 章：第一个 C 程序——从编译器到 OpenSSL 错误队列

本章要解决的是“代码为什么能编译、运行时依赖什么、失败时如何知道原因”。如果只会把 `-lssl -lcrypto` 粘到命令行，遇到交叉编译或 provider 加载错误时会很难排查。

## 2.1 编译、链接、运行是三件事

```text
hello.c --编译--> hello.o --链接--> openssl_hello --装载--> 运行时
                              │                         │
                              └─ 需要 libcrypto.so       └─ 需要动态库和 provider 模块
```

编译阶段需要头文件；链接阶段需要 `libcrypto`/`libssl`；运行阶段动态加载器还要能找到库和 provider 模块。

```sh
ldd build/labs/01_hello/openssl_hello
openssl version -d
openssl version -m
```

## 2.2 CMake 中为什么使用 imported target

根目录的 `CMakeLists.txt` 使用：

```cmake
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto SSL)
target_link_libraries(openssl_hello PRIVATE OpenSSL::Crypto)
```

`OpenSSL::Crypto` 是 CMake 的 imported target，携带头文件目录、库文件和必要的链接属性。比手写 `/usr/lib/libcrypto.so` 更容易迁移到交叉编译 sysroot。

## 2.3 C API 的返回值

OpenSSL 常见约定是：`1` 表示成功，`0`/负值表示失败或特殊状态，指针为 `NULL` 表示创建或读取失败，长度通过 `size_t *out_len` 写回。具体函数仍要查文档，不能把所有非零值都当作成功。

TLS 的 `SSL_read`/`SSL_write` 更特殊：返回值还要交给 `SSL_get_error` 分类，`SSL_ERROR_WANT_READ` 不等于连接断开。

## 2.4 对象生命周期：以 `EVP_MD_CTX` 为例

```c
EVP_MD_CTX *ctx = EVP_MD_CTX_new();  /* 创建：应用拥有 ctx */
if (ctx == NULL) { /* 没有对象可释放 */ }
if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
    EVP_MD_CTX_free(ctx);             /* 初始化失败也要释放 */
    return 1;
}
/* Update/Final ... */
EVP_MD_CTX_free(ctx);                 /* 释放一次 */
```

真实函数往往同时拥有多个对象，推荐用 `goto done` 统一清理。清理标签中调用 `*_free(NULL)` 通常是安全的，可以减少重复分支。

## 2.5 错误队列是线程局部的“原因链”

```c
if (some_op() != 1) {
    fprintf(stderr, "some_op failed\\n");
    ERR_print_errors_fp(stderr);
    goto done;
}
```

错误队列记录库名、原因和可能的文件行信息，并且通常属于当前线程。出现错误后应尽快读取；不要只打印“失败”，也不要把错误队列跨线程当作业务错误码传递。

## 2.6 实验：阅读并运行 `hello.c`

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target openssl_hello
./build/labs/01_hello/openssl_hello
```

请按以下顺序阅读 [hello.c](/home/hong/testcode/openssl_learning/labs/01_hello/hello.c)：

1. `OpenSSL_version` 读取运行时版本；
2. `RAND_bytes` 写入调用方提供的 16 字节缓冲区；
3. 零长度请求是合法 no-op；
4. 未知 EVP 控制命令用于演示错误队列；
5. 最后释放 `EVP_CIPHER_CTX`。

## 2.7 练习

- 把随机数缓冲区改成 32 字节，并打印 hex；
- 在每个失败分支增加对象释放，使用 AddressSanitizer 构建；
- 注释掉 `EVP_CIPHER_CTX_free(ctx)`，再用 Valgrind 观察泄漏。
