#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${1:-build}
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CPP_DIR="$ROOT_DIR/../cpp"
BUILD_PATH="$CPP_DIR/$BUILD_DIR"

mkdir -p "$BUILD_PATH"
cd "$BUILD_PATH"

cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release

echo
echo "Built binaries in: $BUILD_PATH"
