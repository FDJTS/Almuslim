#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${BUILD_DIR:-build}
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CPP_DIR="$ROOT_DIR/../cpp"
BUILD_PATH="$CPP_DIR/$BUILD_DIR"

if [[ -x "$BUILD_PATH/al-muslim" ]]; then
  exec "$BUILD_PATH/al-muslim" "$@"
elif [[ -x "$BUILD_PATH/Release/al-muslim" ]]; then
  exec "$BUILD_PATH/Release/al-muslim" "$@"
else
  echo "Executable not found. Build first with ./build.sh" >&2
  exit 1
fi
