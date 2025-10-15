#!/usr/bin/env bash
set -euo pipefail
# Create a minimal .app bundle that launches the CLI in Terminal
# Usage: ./pack_app.sh [build_dir]

BUILD_DIR=${BUILD_DIR:-${1:-build}}
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CPP_DIR="$ROOT_DIR/../cpp"
BUILD_PATH="$CPP_DIR/$BUILD_DIR"
APP_NAME="Almuslim"
APP_DIR="$ROOT_DIR/packaging/${APP_NAME}.app"
CONTENTS="$APP_DIR/Contents"
MACOS="$CONTENTS/MacOS"
RES="$CONTENTS/Resources"

if [[ ! -x "$BUILD_PATH/al-muslim" && ! -x "$BUILD_PATH/Release/al-muslim" ]]; then
  echo "Build missing. Run ../build.sh first." >&2
  exit 1
fi

rm -rf "$APP_DIR"
mkdir -p "$MACOS" "$RES"

# Info.plist
cat > "$CONTENTS/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>${APP_NAME}</string>
  <key>CFBundleIdentifier</key><string>io.almuslim.cli</string>
  <key>CFBundleVersion</key><string>1.0.0</string>
  <key>CFBundleExecutable</key><string>${APP_NAME}</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.utilities</string>
  <key>LSMinimumSystemVersion</key><string>10.13</string>
  <key>LSUIElement</key><false/>
</dict>
</plist>
PLIST

# Minimal launcher that opens Terminal and runs the bundled binary
cat > "$MACOS/${APP_NAME}" <<'LAUNCH'
#!/usr/bin/env bash
set -euo pipefail
APP_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$APP_DIR/Resources/al-muslim"
# Open a new Terminal window executing the CLI
osascript <<OSA
 tell application "Terminal"
  activate
  do script quoted form of "$BIN"
 end tell
OSA
LAUNCH
chmod +x "$MACOS/${APP_NAME}"

# Copy binary and data under Resources
BIN_SRC="$BUILD_PATH/al-muslim"
if [[ ! -x "$BIN_SRC" ]]; then BIN_SRC="$BUILD_PATH/Release/al-muslim"; fi
cp "$BIN_SRC" "$RES/al-muslim"
if [[ -d "$BUILD_PATH/data" ]]; then
  cp -R "$BUILD_PATH/data" "$RES/data"
fi

echo "Created ${APP_DIR}"
