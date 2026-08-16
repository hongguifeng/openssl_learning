# STM32/FreeRTOS 适配模板

这里给出“需要由产品工程实现”的最小边界，而不是假装一个通用的 FreeRTOS 端口已经完成。建议把 `openssl_freertos_port.h` 的函数映射到：

- `SemaphoreHandle_t`/互斥量：只保护应用自己共享的 `SSL_CTX`、证书缓存或硬件密钥会话；
- `xTaskGetTickCount()`：转换成单调毫秒计时，用于 socket/TLS 超时；
- STM32 RNG HAL 或安全芯片：实现 `openssl_port_get_entropy`，失败时必须让 TLS 启动失败；
- `pvPortMalloc/vPortFree`：只有在经过峰值和碎片测试后才考虑接入，不能盲目把所有分配替换成 FreeRTOS heap。

移植验收至少包括：启动熵源自检、TLS 握手峰值、任务栈水位、网络短读写、证书时间异常、断电重启后的 nonce/证书状态。

