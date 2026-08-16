# 第 1 章：OpenSSL CLI

CLI 是观察 OpenSSL 数据模型的最快方式。后续 C API 实验会使用同样的对象：摘要、密钥、CSR、证书和 TLS 连接。

## 摘要与随机数

```sh
printf 'hello openssl\n' > /tmp/openssl-learning-input.txt
openssl dgst -sha256 /tmp/openssl-learning-input.txt
openssl rand -hex 16
```

`dgst` 输出的是摘要；`rand` 输出的是随机字节，不能把它当作密码或密钥存储方案。

## 生成实验密钥和证书

```sh
umask 077
work=$(mktemp -d)
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out "$work/device.key"
openssl req -new -x509 -sha256 -days 1 \
  -key "$work/device.key" \
  -subj '/CN=openssl-learning-device' \
  -addext 'subjectAltName=DNS:localhost' \
  -out "$work/device.crt"
openssl x509 -in "$work/device.crt" -noout -subject -issuer -dates -text | sed -n '1,18p'
```

## 验证

```sh
openssl pkey -in "$work/device.key" -pubout -out "$work/pub.pem"
openssl x509 -in "$work/device.crt" -pubkey -noout > "$work/cert-pub.pem"
diff -u "$work/pub.pem" "$work/cert-pub.pem"
```

`diff` 无输出才表示证书中的公钥与私钥匹配。实验结束后可删除临时目录：`rm -rf "$work"`。

