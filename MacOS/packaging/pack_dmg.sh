#!/usr/bin/env bash
set -euo pipefail
# Create a DMG containing Almuslim.app
# Usage: ./pack_dmg.sh

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
APP_NAME="Almuslim"
APP_PATH="$ROOT_DIR/${APP_NAME}.app"
DMG="${APP_NAME}.dmg"

if [[ ! -d "$APP_PATH" ]]; then
  echo "${APP_PATH} not found. Run ./pack_app.sh first." >&2
  exit 1
fi

rm -f "$DMG"
hdiutil create -volname "${APP_NAME}" -srcfolder "$APP_PATH" -ov -format UDZO "$DMG"
echo "Created $DMG"
