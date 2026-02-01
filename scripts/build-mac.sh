#!/usr/bin/env bash
# Build manga-reader natively (for the host machine).
# On macOS, requires: brew install meson ninja gtk+ curl libxml2
set -euo pipefail

cd "$(dirname "$0")/.."

# On macOS, curl and libxml2 are keg-only — help pkg-config find them.
if [ "$(uname)" = "Darwin" ] && command -v brew &>/dev/null; then
  export PKG_CONFIG_PATH="/opt/homebrew/opt/curl/lib/pkgconfig:/opt/homebrew/opt/libxml2/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
fi

if ! command -v meson &>/dev/null; then
  echo "Error: meson not found. Install it first:"
  echo "  macOS:  brew install meson ninja"
  echo "  Linux:  apt install meson ninja-build"
  exit 1
fi

if [ -d builddir ]; then
  meson setup builddir --wipe
else
  meson setup builddir
fi

ninja -C builddir

echo ""
echo "Build complete: builddir/manga-reader"

echo "Launching manga-reader..."
exec ./builddir/manga-reader
