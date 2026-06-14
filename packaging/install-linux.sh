#!/usr/bin/env bash
# Install FREEQUENCY on Linux for the current user: copies the binary, registers
# the desktop launcher, app icon and the .freq file association.
#
# Run AFTER building (scripts/build.sh). Usage: ./packaging/install-linux.sh
set -euo pipefail

cd "$(dirname "$0")/.."

BIN="build/src/FREEQUENCY_artefacts/Release/FREEQUENCY"
if [ ! -f "$BIN" ]; then
  echo "Binary not found at $BIN — build first with ./scripts/build.sh" >&2
  exit 1
fi

PREFIX="$HOME/.local"
mkdir -p "$PREFIX/bin" \
         "$PREFIX/share/applications" \
         "$PREFIX/share/icons/hicolor/256x256/apps" \
         "$PREFIX/share/mime/packages"

install -m 755 "$BIN" "$PREFIX/bin/FREEQUENCY"
install -m 644 packaging/freequency-256.png "$PREFIX/share/icons/hicolor/256x256/apps/freequency.png"
install -m 644 packaging/freequency.desktop  "$PREFIX/share/applications/freequency.desktop"
install -m 644 packaging/freequency-mime.xml "$PREFIX/share/mime/packages/freequency.xml"

# Refresh desktop/mime caches (best-effort).
update-desktop-database "$PREFIX/share/applications" 2>/dev/null || true
update-mime-database    "$PREFIX/share/mime"          2>/dev/null || true
gtk-update-icon-cache   "$PREFIX/share/icons/hicolor" 2>/dev/null || true

echo "Installed FREEQUENCY to $PREFIX. Ensure $PREFIX/bin is on your PATH."
echo "Double-clicking a .freq file will now open FREEQUENCY."
