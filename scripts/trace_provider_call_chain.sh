#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exe=${1:-"$root_dir/build/labs/08_provider/openssl_provider_demo"}
module_dir=${OPENSSL_MODULES:-"$root_dir/build/labs/08_provider"}

if ! command -v gdb >/dev/null 2>&1; then
    echo 'gdb not installed; use the breakpoints shown in docs/08-provider-fips.md'
    exit 0
fi

if [ ! -x "$exe" ]; then
    echo "missing executable: $exe" >&2
    exit 1
fi

tmp_cmd=$(mktemp)
trap 'rm -f "$tmp_cmd"' EXIT INT TERM
cat >"$tmp_cmd" <<'EOF'
set breakpoint pending on
set pagination off
break EVP_MD_fetch
commands
  silent
  printf "TRACE: EVP_MD_fetch\n"
  bt 4
  continue
end
break toy_update
commands
  silent
  printf "TRACE: provider toy_update\n"
  bt 4
  continue
end
run
EOF

echo "Tracing application -> EVP -> provider (module path: $module_dir)"
OPENSSL_MODULES="$module_dir" gdb -q -batch -x "$tmp_cmd" --args "$exe"

