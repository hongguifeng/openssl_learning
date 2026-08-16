#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root_dir"
./scripts/check_env.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./scripts/check_modern_api.sh
ctest --test-dir build --output-on-failure
echo 'tutorial verification: OK'

