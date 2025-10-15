#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${BUILD_DIR:-build}
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CPP_DIR="$ROOT_DIR/../cpp"
BUILD_PATH="$CPP_DIR/$BUILD_DIR"

mkdir -p "$BUILD_PATH"
cd "$BUILD_PATH"

if command -v ninja >/dev/null 2>&1; then
	cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
else
	cmake -DCMAKE_BUILD_TYPE=Release ..
fi

cmake --build . --config Release -- -j$(sysctl -n hw.logicalcpu 2>/dev/null || printf 2)

echo
echo "Built binaries in: $BUILD_PATH"
