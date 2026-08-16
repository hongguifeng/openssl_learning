# 第 2 章：CMake、链接与错误处理

OpenSSL 3.x 把算法实现隐藏在 provider 中，应用程序应该通过稳定的高层接口访问它们。工程层面仍要正确处理头文件、链接库、运行时模块和错误队列。

## 最小程序

`labs/01_hello/hello.c` 做三件事：打印版本、调用随机数、在故意失败后打印错误栈。构建方式：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target openssl_hello
./build/labs/01_hello/openssl_hello
```

## 生命周期原则

- `OPENSSL_init_crypto()` 和 `OPENSSL_init_ssl()` 通常由库自动完成；显式初始化适合需要配置文件、provider 或独立库上下文的程序。
- 每个 `*_new()` 对应一个 `*_free()`；错误路径也必须释放已经成功创建的对象。
- OpenSSL 错误队列是线程局部的。捕获错误后应立即读取，不能跨线程传递“裸错误码”代替上下文。
- 不要把 `ERR_error_string()` 的静态缓冲区当作跨线程共享存储；使用 `ERR_print_errors_fp()` 或调用方自己的缓冲区。

