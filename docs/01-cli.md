# 第 1 章：先用 CLI 看懂 OpenSSL 的数据对象

命令行不是“玩具”。它可以让你先观察密钥、证书和编码格式，再把同一件事翻译成 C API。建议每次写 C 代码前，先用 CLI 做一次同等操作并保存输出。

## 1.1 命令的共同结构

OpenSSL CLI 的命令通常可以拆成：

```text
openssl <子命令> <输入/算法选项> <输出/格式选项>
```

例如：

```sh
openssl dgst -sha256 message.bin
```

这里 `dgst` 是“摘要工具”，`-sha256` 选择算法，最后一个参数是输入文件。它不会把文件加密，也不会验证发送者身份。

## 1.2 摘要：看见“输入字节”和“输出字节”

```sh
printf 'hello openssl\n' > /tmp/openssl-learning-input.txt
openssl dgst -sha256 /tmp/openssl-learning-input.txt
```

输出中的 64 个十六进制字符代表 32 个字节。命令行显示的是文本表示，C API 得到的是 `unsigned char digest[32]`。这就是后面经常出现的“长度单位”陷阱：`strlen` 计算字符串，不应该用来计算二进制密文或签名长度。

随机数实验：

```sh
openssl rand -hex 16
openssl rand -base64 16
```

两条命令都生成 16 个随机字节，只是显示编码不同。`-hex 16` 输出 32 个 hex 字符，`-base64 16` 输出约 24 个 Base64 字符。显示长度不等于随机字节长度。

## 1.3 生成 EC 私钥、查看公钥

```sh
umask 077
work=$(mktemp -d)
openssl genpkey -algorithm EC \
  -pkeyopt ec_paramgen_curve:P-256 \
  -out "$work/device.key"
openssl pkey -in "$work/device.key" -text -noout | sed -n '1,18p'
openssl pkey -in "$work/device.key" -pubout -out "$work/device.pub.pem"
```

解释：`genpkey` 生成私钥对象；`P-256` 是曲线参数，不是 256 字节密钥；`pkey -pubout` 从私钥导出公钥；`umask 077` 让其他用户默认无法读取私钥文件。

## 1.4 生成证书并理解 SAN

```sh
openssl req -new -x509 -sha256 -days 1 \
  -key "$work/device.key" \
  -subj '/CN=openssl-learning-device' \
  -addext 'subjectAltName=DNS:localhost' \
  -out "$work/device.crt"
openssl x509 -in "$work/device.crt" \
  -noout -subject -issuer -dates -ext subjectAltName
```

这里生成的是自签名证书：`subject` 和 `issuer` 通常相同。现代主机名验证应看 `subjectAltName`，所以本实验只应该对 `localhost` 做名称匹配，不能因为 CN 看起来正确就放过错误主机名。

验证私钥和证书是否包含同一公钥：

```sh
openssl pkey -in "$work/device.key" -pubout -out "$work/key.pub.pem"
openssl x509 -in "$work/device.crt" -pubkey -noout > "$work/cert.pub.pem"
diff -u "$work/key.pub.pem" "$work/cert.pub.pem"
```

`diff` 没有输出才表示公钥一致。实验结束后删除临时目录：`rm -rf "$work"`。

## 1.5 把 CLI 操作映射到 C API

| CLI | C API 主线 | 结果对象 |
| --- | --- | --- |
| `openssl dgst -sha256` | `EVP_MD_CTX` + `EVP_Digest*` | 摘要字节 |
| `openssl genpkey` | `EVP_PKEY_CTX` + `EVP_PKEY_keygen` | `EVP_PKEY` |
| `openssl pkey` | `PEM_read/write_*` 或 `OSSL_DECODER/ENCODER` | 内存中的密钥对象 |
| `openssl x509` | `PEM_read_X509` + `X509_*` | `X509` |
| `openssl s_client` | `SSL_CTX` + `SSL` + BIO/socket | TLS 连接 |

后面每个 C 实验都会先指出对应的 CLI 操作，再解释 C 对象的创建、初始化和释放。

## 1.6 学习检查点

1. 为什么 16 个随机字节用 hex 显示后会变成 32 个字符？
2. 为什么证书实验需要 `subjectAltName`，只设置 CN 不够吗？
3. 私钥、公钥、证书三者分别证明什么？
