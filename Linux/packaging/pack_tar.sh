#!/usr/bin/env bash
set -euo pipefail

# Build Release and package into tar.gz for Linux
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)
CPP_DIR="$ROOT_DIR/cpp"
BUILD_DIR="${BUILD_DIR:-build}"
DIST_DIR="$ROOT_DIR/dist"

mkdir -p "$CPP_DIR/$BUILD_DIR" "$DIST_DIR"
cd "$CPP_DIR/$BUILD_DIR"

# Configure and build
if command -v ninja >/dev/null 2>&1; then
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
else
  cmake -DCMAKE_BUILD_TYPE=Release ..
fi
cmake --build . --config Release -- -j"$(nproc 2>/dev/null || echo 2)"

# Prepare package staging
STAGE="almuslim-linux"
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp -f "al-muslim" "$STAGE/" 2>/dev/null || true
cp -rf "data" "$STAGE/data"

# Minimal README
cat > "$STAGE/README.txt" << 'EOF'
Almuslim (Linux)
-----------------

Run:
  ./al-muslim

Optional flags:
  --setup             First-time setup wizard
  --ask               Choose a city at launch
  --week              Show next 7 days

Notes:
- Keep the 'data' folder next to the binary.
- Make executable: chmod +x al-muslim
EOF

# Detect architecture suffix
ARCH=$(uname -m 2>/dev/null || echo unknown)
OUT="almuslim-linux-${ARCH}.tar.gz"

# Create tar.gz
rm -f "$DIST_DIR/$OUT"
tar -czf "$DIST_DIR/$OUT" "$STAGE"

echo "Created: $DIST_DIR/$OUT"