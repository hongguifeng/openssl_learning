# 第 11 章：调试、测试与性能

## 测试层次

1. 单元测试：EVP 参数、编码、错误路径和 provider 回调；
2. 互操作测试：与 `openssl s_client/s_server`、其他 TLS 栈互连；
3. 负面测试：错误证书、错误 tag、截断记录、超时、内存不足；
4. 资源测试：握手峰值、吞吐、延迟、堆和栈；
5. 工具检查：`-Wall -Wextra -Wpedantic`、ASan/UBSan、Valgrind、静态分析。

当前仓库的最小门禁是：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

生产项目应把相同测试放入 CI，并固定 OpenSSL 版本、配置文件和目标编译器。

