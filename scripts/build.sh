#!/usr/bin/env bash
# Configure, build and run the tests. Usage: scripts/build.sh [--no-plugin]
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$PWD/.venv/bin:$PATH"

PLUGIN=ON
[ "${1:-}" = "--no-plugin" ] && PLUGIN=OFF

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DGF_BUILD_PLUGIN=$PLUGIN > build.log 2>&1 \
  || { tail -30 build.log; exit 1; }
cmake --build build >> build.log 2>&1 \
  || { grep -E "error|Error" build.log | head -30; exit 1; }
ctest --test-dir build --output-on-failure
