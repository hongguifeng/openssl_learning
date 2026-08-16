# 第 7 章：TLS 1.3——从配置对象到握手状态机

TLS 不是“给 socket 套一层加密函数”。它要协商版本和密码套件、验证身份、派生密钥、处理记录序号、重传/关闭和错误状态。OpenSSL 把这些复杂状态放进 `SSL`，应用负责正确配置和驱动它。

## 7.1 `SSL_CTX` 与 `SSL`

```text
SSL_CTX（长期配置）
  ├─ TLS 方法和版本限制
  ├─ 信任根、证书、私钥
  ├─ 回调、会话缓存、ALPN/SNI 策略
  └─ SSL_new() → SSL（一次具体连接）
                       ├─ BIO/socket
                       ├─ 握手状态
                       ├─ 对端证书
                       └─ 应用数据状态
```

一个进程可以用同一个 `SSL_CTX` 创建很多连接，但每个 `SSL` 不能在多个连接之间复用。连接结束后释放 `SSL`，进程退出或配置不再使用时释放 `SSL_CTX`。

## 7.2 TLS 1.3 握手的学习版流程

```text
ClientHello：支持的版本、随机数、密钥共享、ALPN
        ↓
ServerHello：选择参数，随后发送证书和 CertificateVerify
        ↓
客户端验证证书链/主机名，双方完成 Finished 校验
        ↓
应用数据使用派生出的对称密钥保护
```

真实握手还涉及加密扩展、会话恢复、0-RTT、KeyUpdate 和关闭告警。本教程先固定 TLS 1.3、禁用复杂分支，再逐步加入验证和非阻塞处理。

## 7.3 服务器和客户端最小配置顺序

服务器侧：

1. `SSL_CTX_new(TLS_server_method())`；
2. 设置最小协议版本；
3. `SSL_CTX_use_certificate_chain_file` 加载证书链；
4. `SSL_CTX_use_PrivateKey_file` 加载私钥；
5. `SSL_CTX_check_private_key` 确认证书公钥匹配；
6. 创建 `SSL`，绑定 BIO，进入 accept 状态。

客户端侧：

1. `SSL_CTX_new(TLS_client_method())`；
2. 设置协议版本、信任根和主机名；
3. 创建 `SSL`，设置 SNI/ALPN；
4. 绑定 BIO/socket，进入 connect 状态；
5. 驱动 `SSL_connect`/`SSL_do_handshake`，处理返回值。

第 7 章实验为了隔离握手状态机，客户端暂时使用 `SSL_VERIFY_NONE`；这只用于本地演示，生产客户端必须配置真实信任根和主机名校验。

## 7.4 非阻塞返回值

```c
int ret = SSL_read(ssl, buf, sizeof(buf));
if (ret > 0) {
    /* ret 是实际读取的字节数 */
} else {
    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        /* 注册事件，稍后继续；不是连接失败 */
    } else {
        /* 关闭、证书/协议错误或致命 I/O 错误 */
    }
}
```

不能用 `if (ret <= 0) close()` 代替错误分类，否则非阻塞连接会被错误地当成失败。

## 7.5 实验：本地 TLS 1.3 回环

```sh
cmake --build build --target openssl_tls_loopback
ctest --test-dir build -R tls_loopback --output-on-failure
```

脚本临时生成证书，程序用 BIO pair 连接客户端和服务端，完成握手后交换 `ping`/`pong`。阅读 [tls_loopback.c](/home/hong/testcode/openssl_learning/labs/07_tls/tls_loopback.c) 时画出这条数据流：

```text
SSL_do_handshake(client) → client BIO → server BIO → SSL_do_handshake(server)
SSL_write(client)        → BIO pair  → SSL_read(server)
```

## 7.6 进一步练习

- 把客户端从 `SSL_VERIFY_NONE` 改为信任临时 CA，并设置 `localhost`；
- 把 BIO pair 替换成非阻塞 socket；
- 增加 ALPN，验证客户端和服务器协议选择；
- 人为损坏证书、私钥或 SAN，记录错误队列和 `SSL_get_verify_result`。
