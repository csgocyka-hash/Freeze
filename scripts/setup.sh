#!/usr/bin/env bash
# One-time toolchain setup. Needs no admin rights: everything lives in this folder.
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -x .venv/bin/cmake ]; then
  python3 -m venv .venv
  .venv/bin/pip install --quiet --upgrade pip
  .venv/bin/pip install --quiet cmake ninja
fi

if [ ! -d external/JUCE ]; then
  mkdir -p external
  git -c advice.detachedHead=false clone --quiet --depth 1 --branch 8.0.15 \
    https://github.com/juce-framework/JUCE.git external/JUCE
fi

echo "Toolchain ready: $(.venv/bin/cmake --version | head -1), ninja $(.venv/bin/ninja --version)"
