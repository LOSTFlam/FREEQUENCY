#!/usr/bin/env bash
# Build FREEQUENCY on Linux / macOS.
#
# Usage: ./scripts/build.sh [Release|Debug] [--no-lto]
set -euo pipefail

BUILD_TYPE="${1:-Release}"
LTO="ON"
for arg in "$@"; do
  [ "$arg" = "--no-lto" ] && LTO="OFF"
done

cd "$(dirname "$0")/.."

# Prefer GCC on Linux (some images default `c++` to clang, which can't find libstdc++).
CMAKE_ARGS=(-B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DFREEQUENCY_ENABLE_LTO="$LTO")
if [ "$(uname)" = "Linux" ] && command -v g++ >/dev/null 2>&1; then
  CMAKE_ARGS+=(-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++)
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build build --config "$BUILD_TYPE" --target FREEQUENCY --parallel

echo
echo "Built: build/src/FREEQUENCY_artefacts/$BUILD_TYPE/FREEQUENCY"
