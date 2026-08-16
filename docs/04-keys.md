# 第 4 章：密钥、签名与序列化——不要把“密钥文件”当成一串字符串

这一章解决三个容易混淆的问题：

- `EVP_PKEY` 里面到底装了什么？
- 生成密钥、使用密钥、保存密钥分别由哪些 API 负责？
- 为什么 PEM、DER、PKCS#8、SubjectPublicKeyInfo 会同时出现？

## 4.1 公钥密码学的角色

以签名为例：

```text
发送方：私钥 + 消息 ──签名──> signature
接收方：公钥 + 消息 + signature ──验签──> 通过/失败
```

私钥必须保密，公钥可以分发。签名证明“能够使用对应私钥的一方产生了签名”，但不自动证明这个公钥属于哪个设备；公钥和身份的绑定由证书/PKI 完成。

签名不是加密。对于设备遥测上报，常见方案是 TLS 保护传输，再视业务需要对固件/配置做离线签名。

## 4.2 `EVP_PKEY_CTX` 和 `EVP_PKEY` 的分工

```text
EVP_PKEY_CTX：一次密钥生成/签名参数操作的上下文
      │ keygen_init、设置曲线、keygen
      ▼
EVP_PKEY：生成出来的密钥对象，可用于签名、导出、比较公钥
```

`EVP_PKEY_CTX` 通常是临时对象，完成操作后释放；`EVP_PKEY` 是业务对象，可能要在多个函数间传递。OpenSSL 3.x 不鼓励应用访问 EC/RSA 内部结构体字段，应通过 `EVP_PKEY_*` 和参数接口操作。

## 4.3 EC 签名的调用流程

```text
EVP_DigestSignInit(ctx, ..., digest, ..., private_key)
  → EVP_DigestSignUpdate(ctx, data, len)
  → EVP_DigestSignFinal(ctx, NULL, &sig_len)  // 先询问长度
  → EVP_DigestSignFinal(ctx, sig, &sig_len)   // 再写入缓冲区
```

第二次 `Final` 的“两阶段长度查询”非常重要：ECDSA 签名是 DER 编码，长度不是固定值。不要猜一个刚好够大的数组；生产代码应先查询长度，再分配并检查溢出。

验签时使用公钥和同样的摘要算法，`EVP_DigestVerifyFinal` 返回 1 才表示签名有效。返回 0 是“签名不匹配”，不一定是库故障；只有错误队列能进一步说明解析/参数错误。

## 4.4 PEM、DER 和 PKCS#8

- **DER**：ASN.1 的二进制编码，适合协议和紧凑存储；
- **PEM**：DER 的 Base64 文本封装，加上 `BEGIN/END` 标记；
- **PKCS#8**：私钥容器格式，可携带算法标识和口令加密；
- **SubjectPublicKeyInfo**：通用公钥容器，证书中的公钥通常采用这个结构。

`PEM_read_bio_PrivateKey` 读的是“容器到内存对象”，不是简单读文本。口令保护私钥时，口令回调和错误处理必须由产品设计，不要把口令硬编码在源码中。

## 4.5 实验：逐行阅读 `key_demo.c`

```sh
cmake --build build --target openssl_key_demo
./build/labs/04_keys/openssl_key_demo
```

实验流程是：

1. 创建 EC 密钥生成上下文；
2. 选择 P-256 曲线并生成 `EVP_PKEY`；
3. 用内存 BIO 序列化为 PEM；
4. 从同一个 BIO 读回第二个 `EVP_PKEY`；
5. 用读回的私钥签名；
6. 用原始公钥验签；
7. 在所有错误路径释放对象。

注意 `BIO` 有自己的读写位置：写入 PEM 后再读取，相当于从内存缓冲区的当前位置开始读取。真实文件场景则要考虑权限、截断、损坏和口令错误。

## 4.6 嵌入式注意事项

- 设备私钥尽量不导出到普通文件；优先使用安全芯片或受保护存储；
- 如果硬件只能做“签名操作”而不能导出私钥，需要把 `EVP_PKEY`/provider 适配到硬件 key handle；
- 证书和公钥可以缓存，但必须明确更新和吊销策略；
- 不要用固定测试 key 作为设备身份；
- 释放前是否清零由对象类型和平台策略决定，不能用 `memset` 代替安全密钥生命周期设计。

## 4.7 学习检查点

1. 为什么签名长度不能总是假设为固定值？
2. `EVP_PKEY_CTX` 和 `EVP_PKEY` 谁是临时对象，谁是业务对象？
3. 证书验证解决了“公钥属于谁”，还是只解决了“签名是否数学上正确”？
