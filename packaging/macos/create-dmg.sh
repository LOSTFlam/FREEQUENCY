#!/usr/bin/env bash
# Create FREEQUENCY-0.1.0-macOS.dmg from a Release build.
set -euo pipefail

cd "$(dirname "$0")/../.."
VERSION="${1:-0.1.0}"
APP="build/src/FREEQUENCY_artefacts/Release/FREEQUENCY.app"
STAGING="dist/macos-staging"
DMG="dist/FREEQUENCY-${VERSION}-macOS.dmg"

if [ ! -d "$APP" ]; then
  echo "Build first: ./scripts/build.sh" >&2
  exit 1
fi

rm -rf "$STAGING" dist/*.dmg 2>/dev/null || true
mkdir -p "$STAGING" dist
cp -R "$APP" "$STAGING/"
ln -sf /Applications "$STAGING/Applications"

hdiutil create -volname "FREEQUENCY" -srcfolder "$STAGING" -ov -format UDZO "$DMG"
rm -rf "$STAGING"
echo "Created $DMG"
