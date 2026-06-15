#!/usr/bin/env bash
# Package FREEQUENCY for the current platform (run after ./scripts/build.sh).
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="$(grep -E '^project\(FREEQUENCY VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
echo "Packaging FREEQUENCY $VERSION..."

case "$(uname -s)" in
  Linux)
    bash packaging/linux/build-packages.sh "$VERSION"
    ;;
  Darwin)
    bash packaging/macos/create-dmg.sh "$VERSION"
    ;;
  *)
    echo "Use scripts/package.bat on Windows" >&2
    exit 1
    ;;
esac

echo "Packages in dist/"
