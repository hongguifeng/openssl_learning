#ifndef OPENSSL_FREERTOS_PORT_H
#define OPENSSL_FREERTOS_PORT_H

#include <stddef.h>
#include <stdint.h>

/*
 * OpenSSL 适配层的产品边界。这里不直接包含 FreeRTOS.h，便于先在主机上
 * 做单元测试；目标工程再把 opaque 指针映射为 SemaphoreHandle_t 等类型。
 */
typedef struct openssl_port_mutex *openssl_port_mutex_t;

openssl_port_mutex_t openssl_port_mutex_create(void);
int openssl_port_mutex_lock(openssl_port_mutex_t mutex, uint32_t timeout_ms);
int openssl_port_mutex_unlock(openssl_port_mutex_t mutex);
void openssl_port_mutex_destroy(openssl_port_mutex_t mutex);

uint64_t openssl_port_now_ms(void);
int openssl_port_get_entropy(unsigned char *out, size_t out_len);

#endif

