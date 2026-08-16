#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
status=0
for pattern in 'ENGINE_' 'SHA256_Init' 'SHA256_Update' 'AES_set_' 'RSA_sign'; do
    if rg -n "$pattern" "$root_dir/labs" --glob '*.c' --glob '*.h' >/dev/null 2>&1; then
        echo "legacy API found: $pattern" >&2
        rg -n "$pattern" "$root_dir/labs" --glob '*.c' --glob '*.h' >&2 || true
        status=1
    fi
done
if [ "$status" -ne 0 ]; then
    exit "$status"
fi
echo 'modern API scan: no banned legacy symbols in labs'

