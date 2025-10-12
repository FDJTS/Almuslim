#!/usr/bin/env bash
set -euo pipefail
# Build a simple AppImage for Almuslim CLI using appimagetool if available.
# Fallback: produce a tar.gz with a .desktop and icon.

BUILD_DIR=${BUILD_DIR:-build}
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CPP_DIR="$ROOT_DIR/../cpp"
BUILD_PATH="$CPP_DIR/$BUILD_DIR"
APPDIR="$ROOT_DIR/packaging/AppDir"
APP_NAME="Almuslim"

if [[ ! -x "$BUILD_PATH/al-muslim" && ! -x "$BUILD_PATH/Release/al-muslim" ]]; then
  echo "Build missing. Run ../build.sh first." >&2
  exit 1
fi

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/512x512/apps"

# Copy binary and data
BIN_SRC="$BUILD_PATH/al-muslim"; [[ -x "$BIN_SRC" ]] || BIN_SRC="$BUILD_PATH/Release/al-muslim"
cp "$BIN_SRC" "$APPDIR/usr/bin/al-muslim"
if [[ -d "$BUILD_PATH/data" ]]; then cp -R "$BUILD_PATH/data" "$APPDIR/usr/bin/data"; fi

# Desktop file
cat > "$APPDIR/usr/share/applications/almuslim.desktop" <<DESK
[Desktop Entry]
Name=Almuslim
Comment=Fast terminal prayer times
Exec=al-muslim
Terminal=true
Type=Application
Categories=Utility;
Icon=almuslim
DESK

# Minimal placeholder icon from the SVG wordmark if present
ICON512="$APPDIR/usr/share/icons/hicolor/512x512/apps/almuslim.png"
convert -background none "$ROOT_DIR/../../App/Assets/logo/al-muslim-symbol.svg" -resize 512x512 "$ICON512" 2>/dev/null || true

# AppRun launcher
cat > "$APPDIR/AppRun" <<'RUN'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "$0")" && pwd)"
export PATH="$HERE/usr/bin:$PATH"
exec "$HERE/usr/bin/al-muslim" "$@"
RUN
chmod +x "$APPDIR/AppRun"

if command -v appimagetool >/dev/null 2>&1; then
  (cd "$ROOT_DIR/packaging" && appimagetool AppDir "${APP_NAME}.AppImage")
  echo "Created ${APP_NAME}.AppImage"
else
  (cd "$APPDIR" && tar -czf ../${APP_NAME}-portable.tar.gz .)
  echo "appimagetool not found; created ${APP_NAME}-portable.tar.gz instead"
fi
