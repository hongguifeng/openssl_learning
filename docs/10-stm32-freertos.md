# 第 10 章：STM32 + FreeRTOS 集成

## 分层建议

```text
产品任务（协议/重连/证书轮换）
  ↓
TLS 适配层（SSL_CTX、BIO/socket、超时和事件）
  ↓
OpenSSL libssl/libcrypto
  ↓
系统适配层（FreeRTOS 互斥、堆、时间、网络、TRNG）
  ↓
STM32 HAL/硬件
```

## 必须明确的适配点

- **熵源**：把 STM32 RNG 或安全芯片接入 RAND provider/平台熵入口，并在启动时验证可用性；伪随机或固定种子不可接受。
- **内存**：测量 TLS 握手峰值、任务栈和堆碎片；必要时提供受控分配器并限制并发连接数。
- **线程**：OpenSSL 3.x 自身管理大部分内部同步，但应用仍要保证同一个 `SSL`/上下文对象不被多个任务无保护地并发访问。
- **时间**：证书验证依赖可信时间；无 RTC 的设备需要安全时间同步或受控的证书策略。
- **网络**：把阻塞 socket 适配为 FreeRTOS+TCP/LwIP 的超时与事件模型，正确传播 WANT_READ/WANT_WRITE。

本章正式实现时会提供不依赖具体板卡的适配接口头文件、FreeRTOS 伪实现和交叉编译模板；真正的 HAL、网卡和安全启动集成必须在目标工程完成。

## 实验：先验证适配边界，再接入目标板

1. 用 `cc -fsyntax-only embedded/openssl_freertos_port.h` 检查接口头文件；
2. 在目标工程实现四类 hook，并为熵源失败、互斥超时和时间回退写单元测试；
3. 用 `cmake/toolchains/stm32-gcc-example.cmake` 建立交叉编译工程，补充 MCU、链接脚本、FreeRTOS 和网络栈目标；
4. 在板上记录 TLS 握手期间堆峰值、任务栈水位和 RNG 自检结果，再与主机回环实验结果对照。

本仓库不伪造具体 STM32 型号的 HAL 实现；这一步必须由目标板卡和工具链验证。
