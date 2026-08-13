#!/usr/bin/env bash
# Packages a portable Linux build into dist/GameplayFootball-<ver>-linux-x86_64.tar.gz.
#
# Usage:
#   ./tools/release/package_linux.sh 0.3.0 /path/to/build
#
# Assumes the game was already built in the given build dir (which must contain
# the binary and the data/ copy next to it).

set -euo pipefail

VERSION="${1:?usage: package_linux.sh <version> [build-dir]}"
BUILD_DIR="${2:-build}"
DIST_DIR="${DIST_DIR:-dist}"

if [[ ! -x "$BUILD_DIR/gameplayfootball" ]]; then
  echo "error: no executable at $BUILD_DIR/gameplayfootball (build first)" >&2
  exit 1
fi

stage="$(mktemp -d)/GameplayFootball"
mkdir -p "$stage"
cp -R "$BUILD_DIR/gameplayfootball" "$BUILD_DIR/media" "$BUILD_DIR/databases" "$BUILD_DIR/football.config" "$stage"

cat > "$stage/README.txt" <<'EOF'
Gameplay Football - portable Linux build (x86_64)

Run from a terminal:
    ./gameplayfootball

This archive contains the game binary and its data only; it is NOT fully
self-contained. It was built on Ubuntu 26.04 LTS (SDL3 apt packages exist
since Ubuntu 26.04), so run it on Ubuntu 26.04+ and install the runtime
libraries first:
    sudo apt-get install libsdl3-0 libsdl3-image0 libsdl3-ttf0 libopenal1 \
        libboost-filesystem1.90.0 libboost-thread1.90.0 libsqlite3-0

The game stores its save/league database next to the executable (databases/).
EOF

mkdir -p "$DIST_DIR"
out="$DIST_DIR/GameplayFootball-v${VERSION}-linux-x86_64.tar.gz"
tar czf "$out" -C "$(dirname "$stage")" GameplayFootball
echo "Created $out"
