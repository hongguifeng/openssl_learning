# 第 4 章：密钥与序列化

`EVP_PKEY` 是公钥对象的统一容器。RSA、EC、Ed25519 等算法通过同一套签名/验签和序列化接口暴露，应用代码不应直接访问算法结构体字段。

## 关键格式

- **PKCS#8**：私钥容器，可带口令加密，推荐用于私钥文件。
- **SubjectPublicKeyInfo**：通用公钥容器。
- **PEM**：带 Base64 和头尾标记的文本封装；DER 是二进制 ASN.1 编码。
- **CSR**：证书签名请求，不是证书，也不包含 CA 的签名结果。

## 实验

`labs/04_keys` 在内存中生成 P-256 密钥，使用 `PEM_write_bio_PrivateKey` 序列化，再读回并对消息签名/验签。它演示了对象所有权和失败路径，不把私钥写入仓库。

```sh
cmake --build build --target openssl_key_demo
./build/labs/04_keys/openssl_key_demo
```

生产代码应配合安全存储、密钥轮换、访问控制和设备唯一身份；示例中的内存密钥只用于教学。

