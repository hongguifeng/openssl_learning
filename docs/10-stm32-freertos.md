# 第 10 章：STM32 + FreeRTOS——从主机实验到设备系统

移植 OpenSSL 不是“把源码编译成 ARM”。你要把 OpenSSL 依赖的四类系统能力接到设备：熵源、时间、内存/线程、网络 I/O，并证明这些能力在资源受限和异常条件下仍然成立。

## 10.1 推荐的分层架构

```text
┌────────────────────────────────────────────┐
│ 产品任务：设备注册、业务协议、重连、轮换   │
├────────────────────────────────────────────┤
│ TLS 适配层：SSL_CTX/SSL、证书、超时、ALPN   │
├────────────────────────────────────────────┤
│ OpenSSL 3.x：libssl/libcrypto/provider      │
├────────────────────────────────────────────┤
│ OS 适配层：FreeRTOS mutex、heap、tick、RNG  │
├────────────────────────────────────────────┤
│ 平台：STM32 HAL、RNG、RTC、LwIP/FreeRTOS+TCP │
└────────────────────────────────────────────┘
```

每一层只拥有自己的责任。比如网络驱动负责收发字节，不应该在驱动里解析证书；TLS 适配层负责把 socket 事件翻译成 `SSL_ERROR_WANT_*`，不应该绕过 SSL 直接操作记录层。

## 10.2 四个必须先验证的系统能力

### 熵源

TLS 私钥、临时密钥和 nonce 都依赖不可预测随机数。启动流程应调用硬件 RNG 自检；如果 RNG 不可用，设备应进入安全故障，不能退回 `rand()` 或固定种子。

### 时间

证书验证需要可信时间。无 RTC 的设备可采用安全时间同步、受信启动时间、证书策略窗口或专用单调计数器，但必须明确“时间不可信时是否允许联网”。

### 内存和任务栈

要测量而不是猜测：

- TLS 握手峰值堆；
- 证书链解析峰值；
- 每个 TLS 任务栈水位；
- 多连接并发的最坏情况；
- 长时间运行后的堆碎片。

### 网络 I/O

LwIP/FreeRTOS+TCP 的 socket 语义必须映射为阻塞、超时或非阻塞 BIO。`SSL_read` 返回 WANT_READ 时，任务应等待 socket 事件或队列，而不是忙循环。

## 10.3 FreeRTOS 适配接口怎么落地

[embedded/openssl_freertos_port.h](/home/hong/testcode/openssl_learning/embedded/openssl_freertos_port.h) 只声明四类 hook：

| hook | FreeRTOS/STM32 实现示例 | 验收条件 |
| --- | --- | --- |
| mutex create/lock/unlock | `SemaphoreHandle_t` | 超时可观察，不死锁 |
| now_ms | `xTaskGetTickCount` 转毫秒 | 单调、不回退、溢出处理 |
| get_entropy | STM32 HAL RNG/安全芯片 | 失败可传播，不能返回固定数据 |
| destroy | `vSemaphoreDelete`/资源释放 | 重连和重启无泄漏 |

头文件故意不包含 `FreeRTOS.h`，这样可以先在主机做接口测试，再在目标工程提供具体实现。

## 10.4 建议的移植顺序

1. **主机功能基线**：先通过本仓库 EVP、X.509、TLS 和负面测试；
2. **交叉编译最小库**：使用 [stm32-gcc-example.cmake](/home/hong/testcode/openssl_learning/cmake/toolchains/stm32-gcc-example.cmake)，固定 sysroot 和编译选项；
3. **平台 hook**：先实现时间和 RNG，再接内存、互斥和网络；
4. **单任务 TLS**：只允许一个连接，验证握手、证书和关闭；
5. **异常测试**：RNG 失败、证书过期、短读写、断网、超时、断电；
6. **并发和资源测试**：增加连接数，记录 heap/stack 水位；
7. **硬件加速/provider**：最后接入安全芯片或硬件签名，保持软件回退策略明确。

## 10.5 裁剪与链接

嵌入式项目要在“功能、镜像大小、维护风险”之间取舍。常见决策包括：

- 静态链接还是动态 provider；
- 是否保留 legacy provider；
- 只保留 TLS 1.3、EC、AES-GCM、SHA-256 等必需算法；
- 是否启用汇编优化、硬件 AES/RNG；
- 文件系统不可用时如何加载配置和证书；
- 错误日志如何脱敏并保存。

不要为了减小镜像直接删除随机数、证书验证或错误处理；应先建立功能测试和资源预算。

## 10.6 学习检查点

1. RNG 不可用时为什么不能“先用固定种子，联网后再换”？
2. `SSL_ERROR_WANT_READ` 在 FreeRTOS 任务中应该映射成什么动作？
3. 为什么建议先做单连接，再做并发连接？
4. 目标板没有文件系统时，provider 和证书配置从哪里来？
