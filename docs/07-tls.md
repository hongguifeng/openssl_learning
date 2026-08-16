# 第 7 章：TLS 1.3 客户端与服务端

TLS 1.3 的核心流程可以抽象为：客户端 Hello → 服务端选择参数并证明身份 → 双方派生握手/应用密钥 → 加密应用数据。OpenSSL 将握手状态机封装在 `SSL` 对象中，应用主要负责配置 `SSL_CTX`、提供 BIO/socket，并正确处理返回值。

## 回环实验

```sh
cmake --build build --target openssl_tls_loopback
ctest --test-dir build -R tls_loopback --output-on-failure
```

实验会临时生成 P-256 自签名证书，使用内存 BIO 建立客户端/服务端 TLS 1.3 握手，交换 `ping`/`pong`。它不依赖 Docker/Podman，也不连接公网。

## 生产配置清单

- 明确设置最小协议版本（本教程实验固定 TLS 1.3）；
- 配置可信根、主机名、SNI 和 ALPN；
- 处理 `SSL_ERROR_WANT_READ/WRITE`、超时、对端关闭和重连；
- 记录握手失败原因，但不要把私钥、口令或完整应用数据写入日志。

