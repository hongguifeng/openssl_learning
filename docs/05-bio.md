# 第 5 章：BIO 与数据流

BIO 是 OpenSSL 的 I/O 抽象层。它把“数据从哪里来”与“上层协议如何处理”分开，常见类型包括文件 BIO、内存 BIO、socket BIO 和过滤 BIO。

## 重要行为

- `BIO_read`/`BIO_write` 可能只处理部分数据；网络代码不能假设一次调用完成全部传输。
- 非阻塞 TLS 使用 `SSL_get_error` 判断 `SSL_ERROR_WANT_READ` 和 `SSL_ERROR_WANT_WRITE`，再交还事件循环。
- BIO 的释放通常由 `SSL_set_bio` 转移所有权；不要在 SSL 释放后再次释放同一对象。
- 内存 BIO 适合单元测试和协议状态机验证，不等同于真实 socket 的背压和断连行为。

第 7 章的 TLS 回环实验使用 `BIO_new_bio_pair` 在同一进程内模拟两个端点，避免依赖外部网络。

