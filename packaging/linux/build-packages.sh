#!/usr/bin/env bash
# Build Linux .tar.gz and optional .deb (requires dpkg-deb).
set -euo pipefail

cd "$(dirname "$0")/../.."
VERSION="${1:-0.1.0}"
BIN="build/src/FREEQUENCY_artefacts/Release/FREEQUENCY"

if [ ! -f "$BIN" ]; then
  echo "Build first: ./scripts/build.sh" >&2
  exit 1
fi

mkdir -p dist
ARCH="$(uname -m)"
case "$ARCH" in
  x86_64) DEB_ARCH=amd64 ;;
  aarch64|arm64) DEB_ARCH=arm64 ;;
  *) DEB_ARCH="$ARCH" ;;
esac
STAGE="dist/linux-stage-${ARCH}"
rm -rf "$STAGE"
mkdir -p "$STAGE/usr/bin" \
         "$STAGE/usr/share/applications" \
         "$STAGE/usr/share/icons/hicolor/256x256/apps" \
         "$STAGE/usr/share/mime/packages"

install -m 755 "$BIN" "$STAGE/usr/bin/FREEQUENCY"
install -m 644 packaging/freequency.desktop "$STAGE/usr/share/applications/"
install -m 644 packaging/freequency-mime.xml "$STAGE/usr/share/mime/packages/freequency.xml"
install -m 644 packaging/freequency-256.png "$STAGE/usr/share/icons/hicolor/256x256/apps/freequency.png"

TAR="dist/FREEQUENCY-${VERSION}-linux-${ARCH}.tar.gz"
tar -czf "$TAR" -C "$STAGE" usr
echo "Created $TAR (extract to / or use install-linux.sh for user install)"

if command -v dpkg-deb >/dev/null 2>&1; then
  PKG="dist/freequency_${VERSION}_${ARCH}"
  rm -rf "$PKG"
  cp -a "$STAGE" "$PKG"
  mkdir -p "$PKG/DEBIAN"
  cat > "$PKG/DEBIAN/control" <<EOF
Package: freequency
Version: ${VERSION}
Section: sound
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: FREEQUENCY
Description: Hybrid DAW — pattern + linear audio/MIDI production
EOF
  dpkg-deb --build "$PKG" "dist/FREEQUENCY-${VERSION}-linux-${DEB_ARCH}.deb"
  rm -rf "$PKG"
  echo "Created dist/FREEQUENCY-${VERSION}-linux-${DEB_ARCH}.deb"
fi

rm -rf "$STAGE"
