#!/usr/bin/env sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 OPENSSL_SOURCE_DIR OUTPUT.md" >&2
    exit 2
fi

src=$1
out=$2

for required in \
    VERSION.dat \
    include/openssl/evp.h \
    crypto/evp/evp_fetch.c \
    crypto/provider_core.c \
    providers/defltprov.c \
    ssl/ssl_lib.c \
    ssl/statem/statem.c
do
    if [ ! -f "$src/$required" ]; then
        echo "not an expected OpenSSL source tree: missing $required" >&2
        exit 1
    fi
done

major=$(sed -n 's/^MAJOR=//p' "$src/VERSION.dat" | head -1)
minor=$(sed -n 's/^MINOR=//p' "$src/VERSION.dat" | head -1)
patch=$(sed -n 's/^PATCH=//p' "$src/VERSION.dat" | head -1)
c_files=$(find "$src/crypto" "$src/ssl" "$src/providers" -type f -name '*.c' | wc -l)
h_files=$(find "$src/include" -type f -name '*.h' | wc -l)

{
    echo '# Generated OpenSSL source index'
    echo
    echo "- Source: \`$src\`"
    echo "- Version: \`${major}.${minor}.${patch}\`"
    echo "- C files under crypto/ssl/providers: \`$c_files\`"
    echo "- Header files under include: \`$h_files\`"
    echo
    echo '## Module roots'
    echo
    echo '| Directory | Responsibility |'
    echo '| --- | --- |'
    echo '| `include/openssl` | Installed public API and public types |'
    echo '| `include/internal` | Cross-module internal API; not application ABI |'
    echo '| `crypto/evp` | Algorithm contexts, fetch and provider wrappers |'
    echo '| `crypto/provider_core.c` | Provider objects, activation and Core connection |'
    echo '| `crypto/core_*` | Method construction, names and property infrastructure |'
    echo '| `providers` | Provider entry points and algorithm implementations |'
    echo '| `ssl` | Public SSL API, handshake, record and session logic |'
    echo '| `ssl/statem` | Client/server handshake state machines |'
    echo '| `ssl/record` | TLS records and BIO-facing I/O |'
    echo '| `apps` | openssl command line frontend and public API examples |'
    echo '| `test` | Unit, recipe and regression tests |'
    echo
    echo '## EVP fetch path: symbol locations'
    echo
    echo '```text'
    rg -n --glob '*.c' \
        'EVP_MD_fetch|evp_generic_fetch|inner_evp_generic_fetch|ossl_method_construct' \
        "$src/crypto/evp" "$src/crypto/core_fetch.c" | head -40 || true
    echo '```'
    echo
    echo '## Provider path: symbol locations'
    echo
    echo '```text'
    rg -n --glob '*.c' \
        'OSSL_provider_init|ossl_provider_activate|query_operation|OSSL_FUNC_PROVIDER_QUERY_OPERATION' \
        "$src/crypto/provider_core.c" "$src/providers" | head -50 || true
    echo '```'
    echo
    echo '## TLS handshake and record path: symbol locations'
    echo
    echo '```text'
    rg -n --glob '*.c' \
        'SSL_do_handshake|ossl_statem_connect|state_machine|tls_construct_client_hello|tls_process_server_hello|ssl3_read_bytes' \
        "$src/ssl" | head -60 || true
    echo '```'
    echo
    echo '## Public API declarations'
    echo
    echo '```text'
    rg -n \
        'EVP_MD_fetch|EVP_DigestInit_ex|SSL_do_handshake|SSL_connect|SSL_read_ex|X509_verify_cert' \
        "$src/include/openssl" | head -40 || true
    echo '```'
} > "$out"

printf 'OpenSSL source index written: %s\n' "$out"
