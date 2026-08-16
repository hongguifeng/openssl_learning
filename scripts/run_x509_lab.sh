#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 path/to/openssl_x509_verify" >&2
    exit 2
fi
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT INT TERM
umask 077
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out "$tmp_dir/device.key" >/dev/null 2>&1
openssl req -new -x509 -sha256 -days 1 -key "$tmp_dir/device.key" \
    -subj '/CN=openssl-learning-device' \
    -addext 'subjectAltName=DNS:localhost' \
    -out "$tmp_dir/device.crt" >/dev/null 2>&1
"$1" "$tmp_dir/device.crt" localhost
if "$1" "$tmp_dir/device.crt" wrong.example; then
    echo 'wrong hostname was accepted' >&2
    exit 1
fi
echo 'hostname negative test: rejected as expected'

