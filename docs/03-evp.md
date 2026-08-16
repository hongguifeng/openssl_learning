# 第 3 章：EVP 抽象层——现代密码 API 主线

OpenSSL 3.x 的应用代码应尽量停留在 EVP 层。EVP 把“算法名称、参数、上下文、实现来源”分开，应用不需要依赖 provider 的内部结构体。

## 3.1 摘要上下文

一个摘要操作通常是：

```text
EVP_MD_CTX_new
  -> EVP_DigestInit_ex(ctx, EVP_sha256(), NULL)
  -> EVP_DigestUpdate(ctx, chunk, chunk_len)  // 可重复调用
  -> EVP_DigestFinal_ex(ctx, digest, &digest_len)
  -> EVP_MD_CTX_free
```

`Update` 可以接收任意大小的分块，因此适合文件、网络帧和 FreeRTOS 环形缓冲区。摘要只提供完整性指纹，不提供来源认证。

## 3.2 AEAD：AES-GCM

AES-GCM 的接口顺序要特别注意：

1. 先设置 key 和 IV；
2. 设置 IV 长度（非 12 字节时必须显式设置）；
3. 通过 `EVP_EncryptUpdate` 输入 AAD；
4. 输入明文得到密文；
5. `EVP_EncryptFinal_ex` 后读取 tag；
6. 解密时在 `EVP_DecryptFinal_ex` 前设置 tag，返回值为 0 表示认证失败。

同一个 key 下绝对不能重复使用 GCM nonce。嵌入式设备通常需要把单调计数器或随机 nonce 与密文一起保存，并处理掉电回滚问题。

## 实验

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target openssl_evp_demo
./build/labs/03_evp/openssl_evp_demo
```

程序会打印 SHA-256、AES-GCM 密文和解密结果，然后篡改密文并确认认证失败。可用 CLI 交叉检查摘要：

```sh
printf 'openssl-evp-demo' | openssl dgst -sha256
```

