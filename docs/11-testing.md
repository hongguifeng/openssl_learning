# 第 11 章：调试、测试与性能——让“安全”变成可证据化的结果

密码学代码最危险的状态是“正常路径能跑，异常路径没人测”。本章把测试拆成可重复的层级，并说明每一层能证明什么、不能证明什么。

## 11.1 测试金字塔

```text
                 互操作/真实设备
              ┌──────────────────┐
              │ TLS、证书轮换、断电 │
              ├──────────────────┤
              │ 集成：BIO/网络/Provider│
              ├──────────────────┤
              │ 单元：EVP/参数/错误    │
              └──────────────────┘
```

单元测试不能证明网络超时正确；TLS 回环不能证明真实网卡的短读写正确；主机测试不能证明 STM32 的 RNG 或堆峰值满足要求。每一层都要明确边界。

## 11.2 当前仓库的门禁

```sh
./scripts/verify_tutorial.sh
```

它依次完成环境检查、CMake 配置、全量构建、现代 API 扫描和 CTest。当前 CTest 包含：

- `openssl_hello`：版本、随机数、错误路径；
- `openssl_evp_demo`：分块 SHA-256、AES-GCM 和错误 tag；
- `openssl_key_demo`：EC 密钥、PEM、签名验签；
- `x509_verify`：正确/错误主机名；
- `tls_loopback`：TLS 1.3 BIO 回环；
- `openssl_provider_demo`：自定义 provider fetch 和回调。

## 11.3 故障注入清单

至少为每个模块增加：

- `NULL`/长度为零/超长输入；
- 内存分配失败；
- 损坏 PEM/DER；
- 错误口令、错误签名、错误 GCM tag；
- 证书过期、SAN 错误、缺失中间证书；
- socket 短读写、超时、对端提前关闭；
- provider 不存在、属性不匹配、硬件返回错误；
- FreeRTOS mutex 超时、tick 回绕和 RNG 故障。

## 11.4 调试工具怎么用

- `ERR_print_errors_fp`：立即打印 OpenSSL 错误队列；
- GDB：在 `EVP_MD_fetch`、`SSL_do_handshake`、provider 回调设断点；
- ASan/UBSan：检查越界、use-after-free、未定义行为；
- Valgrind：检查主机实验泄漏；
- `nm/ldd/readelf`：确认符号、依赖和 provider 模块；
- Wireshark/tshark：只在测试环境观察 TLS 版本、SNI、ALPN，不记录私钥。

## 11.5 性能测量应该记录什么

密码性能至少记录算法、输入大小、吞吐、延迟、CPU 频率、堆峰值和温度/功耗条件。TLS 性能要区分：

- 首次握手；
- 会话恢复；
- 单条小消息延迟；
- 大块连续传输；
- 多连接并发。

不要只报“每秒多少次 AES”，因为设备瓶颈可能在证书解析、网络往返或内存分配。

## 11.6 学习练习

1. 给 `tls_loopback` 增加错误证书测试；
2. 用 ASan 构建全部实验并记录结果；
3. 在 provider 的 `toy_update` 中故意返回失败，观察 EVP 错误如何传播；
4. 在 STM32 上定义“握手最大堆峰值”和“任务最小剩余栈”发布门禁。
