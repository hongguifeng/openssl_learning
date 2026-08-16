#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 OPENSSL_SOURCE_DIR" >&2
    exit 2
fi

src=$1

require_symbol() {
    file=$1
    symbol=$2
    if ! rg -q "$symbol" "$src/$file"; then
        echo "missing expected symbol $symbol in $file" >&2
        exit 1
    fi
    printf 'verified: %-36s %s\n' "$file" "$symbol"
}

version=$(awk -F= '
    /^MAJOR=/ { major=$2 }
    /^MINOR=/ { minor=$2 }
    /^PATCH=/ { patch=$2 }
    END { print major "." minor "." patch }
' "$src/VERSION.dat")

if [ "$version" != "3.0.2" ]; then
    echo "expected OpenSSL 3.0.2 source, got $version" >&2
    exit 1
fi

require_symbol include/openssl/evp.h 'EVP_MD_fetch'
require_symbol crypto/evp/digest.c 'EVP_MD \*EVP_MD_fetch'
require_symbol crypto/evp/evp_fetch.c 'inner_evp_generic_fetch'
require_symbol crypto/core_fetch.c 'ossl_method_construct'
require_symbol crypto/provider_core.c 'ossl_provider_query_operation'
require_symbol providers/defltprov.c 'ossl_default_provider_init'
require_symbol ssl/ssl_lib.c 'int SSL_do_handshake'
require_symbol ssl/statem/statem.c 'int ossl_statem_connect'
require_symbol ssl/statem/statem_clnt.c 'tls_construct_client_hello'
require_symbol ssl/record/rec_layer_s3.c 'int ssl3_read_bytes'

echo 'OpenSSL source structure verification: OK'
