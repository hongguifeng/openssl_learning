# STM32/FreeRTOS 适配模板：每个 hook 都要有可验证的契约

这里不是“把函数名换成 FreeRTOS API”的示例，而是 OpenSSL 与设备系统之间的契约清单。目标工程应该为每个函数写实现、单元测试和故障行为。

## 1. 互斥量契约

`openssl_port_mutex_create/lock/unlock/destroy` 可以映射到 `SemaphoreHandle_t`，但要先决定：

- 哪些对象真的会被多个任务共享；
- lock 是否允许超时，超时后上层如何退出；
- ISR 是否可能调用，若可能就不能使用普通互斥量；
- 删除顺序如何避免任务仍持有句柄。

不要为了“OpenSSL 需要线程安全”就给每个 API 调用套全局锁；应按 `SSL_CTX`、证书缓存、硬件密钥会话等共享对象划分锁。

## 2. 时间契约

`openssl_port_now_ms` 应返回单调时间，用于超时和重连退避；证书有效期需要另一个可解释的墙上时钟。FreeRTOS tick 可能回绕，转换和比较必须使用官方的 tick 回绕安全方式。

## 3. 熵源契约

`openssl_port_get_entropy` 映射 STM32 HAL RNG、安全芯片或专用 TRNG。实现必须：

- 检查硬件状态和超时；
- 失败时返回错误，不填充可预测数据；
- 在启动和运行期定义健康检查；
- 记录可诊断但不泄露随机输出的错误信息。

## 4. 网络与内存不在这个头文件里

网络适配通常在 TLS/BIO 层：把 LwIP/FreeRTOS+TCP 的 socket、事件和超时转换成 `BIO`/`SSL` 所需语义。内存适配则要通过目标 OpenSSL 构建选项或明确的分配接口实现，并用峰值/碎片测试证明可行。

## 5. 建议的目标工程目录

```text
platform/
├── stm32_rng.c              # 硬件熵源
├── freertos_mutex.c         # 互斥量/任务同步
├── monotonic_time.c         # tick 和墙上时间
├── lwip_bio.c               # socket/BIO 适配
└── openssl_provider_hw.c    # 可选硬件 provider
security/
├── tls_client.c             # SSL_CTX/SSL 状态机
├── cert_store.c             # 信任根和轮换
└── key_handle.c              # 安全芯片 key handle
```

## 6. 验收顺序

1. 主机跑通仓库全部 CTest；
2. 目标架构只运行版本、摘要、随机数和错误路径；
3. 接入 RNG/时间，再做证书验证；
4. 接入网络，先单连接 TLS；
5. 注入短读写、超时、断网、证书错误和 RNG 故障；
6. 记录堆/栈峰值、Flash 大小、握手时间和功耗；
7. 最后再加入并发、硬件加速和 OTA 证书轮换。
