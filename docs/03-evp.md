# 第 3 章：EVP——OpenSSL 3.x 应用代码的主干

这一章是后面所有密码实验的基础。EVP 不只是函数名集合，而是 OpenSSL 3.x 的接口设计：应用描述“我要 SHA-256/AES-GCM/签名”，库再根据 provider、属性和配置找到具体实现。

## 3.1 EVP 的对象分层

```text
EVP_sha256()/EVP_MD_fetch()  → EVP_MD
                                  │ 算法描述，不保存一次计算的中间状态
EVP_MD_CTX_new()             → EVP_MD_CTX
                                  │ 一次计算的状态：已处理多少数据、内部缓冲等
EVP_DigestInit/Update/Final  → 对 ctx 的状态机操作
```

对称加密使用 `EVP_CIPHER` 和 `EVP_CIPHER_CTX`；公钥算法使用 `EVP_PKEY` 保存密钥，用 `EVP_MD_CTX` 或 `EVP_PKEY_CTX` 保存一次签名或密钥协商操作。

## 3.2 摘要的完整调用顺序

```c
EVP_MD_CTX *ctx = EVP_MD_CTX_new();
const EVP_MD *sha256 = EVP_sha256();
EVP_DigestInit_ex(ctx, sha256, NULL);
EVP_DigestUpdate(ctx, part1, part1_len);
EVP_DigestUpdate(ctx, part2, part2_len);
EVP_DigestFinal_ex(ctx, digest, &digest_len);
EVP_MD_CTX_free(ctx);
```

`Init` 设置初始状态；`Update` 可以多次调用，所以不需要把整个文件读进 RAM；`Final` 写出摘要并返回长度；`free` 释放一次操作上下文。算法描述对象通常由库管理，不要对 `EVP_sha256()` 返回的对象调用 `free`。

## 3.3 为什么不能把摘要当认证

攻击者可以修改消息并重新计算 SHA-256。接收方只比较自己算出的摘要，无法知道摘要是否来自可信发送者。需要认证时使用 HMAC、数字签名或 TLS，而不是自行拼接“密文 + SHA-256”。

## 3.4 AES-GCM 是一个有状态的 AEAD 操作

```text
EncryptInit(key, iv)
  → EncryptUpdate(AAD)       // 不产生密文，但会被认证
  → EncryptUpdate(plaintext) // 产生 ciphertext
  → EncryptFinal
  → GET_TAG(tag)

DecryptInit(key, iv)
  → DecryptUpdate(AAD)
  → DecryptUpdate(ciphertext)
  → SET_TAG(tag)
  → DecryptFinal                // 返回 0 = 认证失败
```

同一把 key 下不能重复使用同一个 nonce；AAD 不加密但参与认证；解密 `Final` 失败时输出明文必须丢弃；tag 长度和传输格式必须由协议明确规定。

## 3.5 实验：逐步阅读 `evp_demo.c`

```sh
cmake --build build --target openssl_evp_demo
./build/labs/03_evp/openssl_evp_demo
printf 'openssl-evp-demo' | openssl dgst -sha256
```

阅读 [evp_demo.c](/home/hong/testcode/openssl_learning/labs/03_evp/evp_demo.c) 时重点观察：

- 为什么使用 `sizeof(message) - 1`；
- 为什么 GCM 加密完成后必须读取 tag；
- 为什么解密先设置 tag，再调用 `EVP_DecryptFinal_ex`；
- 为什么篡改 tag 后必须把 `Final` 的失败当作认证结果；
- 每个 `goto done` 分支释放了哪些对象。

## 3.6 常见失败排查

| 现象 | 常见原因 | 排查办法 |
| --- | --- | --- |
| 摘要与 CLI 不一致 | 输入长度多了换行或 NUL | 用 `xxd` 检查实际字节 |
| GCM 解密失败 | key/IV/AAD/tag 不一致 | 打印长度和 hex，先固定测试向量 |
| tag 错误仍显示成功 | 没检查 `EVP_DecryptFinal_ex` | 把返回值当作认证结果 |
| 二进制输出被截断 | 使用 `strlen` | 全程使用显式长度 |
| fetch 找不到算法 | provider 未加载或属性不匹配 | 查看 provider 列表和属性字符串 |

## 3.7 学习检查点

1. `EVP_MD` 与 `EVP_MD_CTX` 的生命周期差异是什么？
2. GCM 的 AAD 为什么不出现在密文里却仍然影响 tag？
3. 解密 `Final` 失败时，为什么即使已经得到部分明文也必须全部丢弃？
