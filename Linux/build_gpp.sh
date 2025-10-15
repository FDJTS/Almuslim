#!/usr/bin/env bash
set -euo pipefail

# Build with g++ directly using the Makefile in cpp/
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CPP_DIR="$ROOT_DIR/../cpp"

cd "$CPP_DIR"
make -j"${JOBS:-$(nproc || echo 2)}"

echo
echo "Built: $CPP_DIR/build-gpp/al-muslim"
