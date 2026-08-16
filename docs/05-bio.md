# 第 5 章：BIO——把 I/O 从密码和 TLS 状态机中拆出来

BIO 可以理解为 OpenSSL 的“可替换字节流接口”。上层代码只关心读/写，底层可以是文件、内存、socket、压缩过滤器或两个端点之间的内存管道。

## 5.1 三种常用 BIO

```text
BIO_s_file()       文件
BIO_s_mem()        内存缓冲区
BIO_new_bio_pair() 两个相互连接的内存端点
```

`BIO_new_bio_pair` 很适合测试 TLS 状态机：客户端写入的握手数据会出现在服务端 BIO，服务端响应再流回客户端，不需要打开真实 TCP 端口。

## 5.2 BIO 链和所有权

BIO 可以串成链：

```text
应用 → BIO_f_base64 → BIO_s_mem → 内存
```

调用 `BIO_push(filter, source)` 后，链的拥有关系会改变。释放顶部 BIO 通常会递归释放下面的 BIO；因此不要在链释放后再次释放底层对象。`SSL_set_bio` 也会把 BIO 的所有权交给 `SSL`，这是 TLS 代码最常见的 double-free 来源之一。

## 5.3 短读写和非阻塞

文件 BIO 里一次 `BIO_read` 可能读到请求长度，但 socket BIO 不能这样假设。网络层要处理：

```text
BIO_read/BIO_write
  ├─ > 0：实际处理了多少字节
  ├─ == 0：对端关闭或没有更多数据（结合 BIO 状态判断）
  └─ < 0：错误/需要等待，交给事件循环
```

TLS 进一步用 `SSL_get_error` 把“暂时没有数据”分类为 `SSL_ERROR_WANT_READ` 或 `SSL_ERROR_WANT_WRITE`。FreeRTOS 任务不应该用无限阻塞掩盖超时；应把它转成队列、事件组或 socket 超时。

## 5.4 BIO 与 TLS 的边界

`SSL` 负责协议状态机和加密记录，BIO 只负责搬运字节：

```text
应用消息
  ↓ SSL_write/SSL_read
TLS 记录层
  ↓ BIO_write/BIO_read
socket、网卡驱动或 BIO pair
```

因此“BIO 写成功”不代表对端已经验证证书，也不代表应用消息已经被对端处理；它只表示字节被交给下一层。

## 5.5 实验

第 7 章的 `tls_loopback.c` 使用两个 BIO pair 端点完成 TLS 握手和 ping/pong。运行：

```sh
ctest --test-dir build -R tls_loopback --output-on-failure
```

阅读代码时追踪：

1. 两个 `SSL` 如何分别绑定自己的 BIO；
2. 为什么绑定后本地变量被置为 `NULL`；
3. 握手失败时如何判断是暂时等待还是致命错误；
4. 发送 4 字节消息时为什么仍然检查实际返回值。

## 5.6 学习检查点

1. `BIO` 负责加密吗？
2. `SSL_set_bio` 后谁拥有 BIO？
3. 为什么非阻塞 TLS 需要事件循环，而不是简单地重复调用 `SSL_read`？
