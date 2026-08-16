#!/usr/bin/env sh
set -eu

printf '%s\n' '== OpenSSL tutorial environment =='
command -v openssl >/dev/null || { echo 'missing: openssl'; exit 1; }
command -v cmake >/dev/null || { echo 'missing: cmake'; exit 1; }
command -v cc >/dev/null || { echo 'missing: C compiler'; exit 1; }

openssl version -a
cmake --version | sed -n '1p'
cc --version | sed -n '1p'

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT INT TERM
cat >"$tmp_dir/probe.c" <<'EOF'
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <stdio.h>
int main(void) {
    printf("headers: %s\n", OPENSSL_VERSION_TEXT);
    printf("runtime: %s\n", OpenSSL_version(OPENSSL_VERSION));
    return 0;
}
EOF

cc "$tmp_dir/probe.c" -lcrypto -o "$tmp_dir/probe"
"$tmp_dir/probe"
echo 'environment check: OK'

