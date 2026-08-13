#!/usr/bin/env bash
# Packages a macOS build into a double-clickable .app bundle and zips it into
# dist/GameplayFootball-<ver>-macos-<arch>.zip.
#
# Usage (run on the Mac, from the repo root):
#   ./tools/release/package_macos.sh 0.3.0 [build-dir]
#
# Assumes the game was already built (default build dir: build, arm64 on Apple
# Silicon). Bundles all runtime dylibs (SDL3, SDL3_image, SDL3_ttf, OpenAL,
# Boost, sqlite3) into the app so it runs without Homebrew installed, rewrites
# their install names, and ad-hoc signs the bundle. Not notarized: first launch
# needs right-click -> Open (or `xattr -dr com.apple.quarantine`).

set -euo pipefail

VERSION="${1:?usage: package_macos.sh <version> [build-dir]}"
BUILD_DIR="${2:-build}"
DIST_DIR="${DIST_DIR:-dist}"

BIN="$BUILD_DIR/gameplayfootball"
if [[ ! -x "$BIN" ]]; then
  echo "error: no executable at $BIN (build first)" >&2
  exit 1
fi
ARCH="$(uname -m)"  # arm64 / x86_64

APP="GameplayFootball.app"
STAGE="$(mktemp -d)/$APP"
CONTENTS="$STAGE/Contents"
mkdir -p "$CONTENTS/MacOS" "$CONTENTS/Resources/data" "$CONTENTS/Frameworks"

# The game resolves all paths (football.config, databases/, media/, log.txt)
# relative to the CWD, but macOS launches a .app bundle from "/". So the
# real binary is renamed to gameplayfootball-bin and a launcher script takes
# its place: it chdir's into Contents/Resources/data before exec'ing the game,
# making the bundle double-clickable.
cp "$BIN" "$CONTENTS/MacOS/gameplayfootball-bin"
cat > "$CONTENTS/MacOS/gameplayfootball" <<'EOF'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR/../Resources/data" || exit 1
exec "$DIR/gameplayfootball-bin" "$@"
EOF
chmod +x "$CONTENTS/MacOS/gameplayfootball"
cp -R "$BUILD_DIR/media" "$BUILD_DIR/databases" "$BUILD_DIR/football.config" "$CONTENTS/Resources/data"

# --- bundle runtime dylibs ------------------------------------------------
# Collect the dylib closure of the binary and re-point everything at the
# bundled copies (Contents/Frameworks) so the app is self-contained.

brew_prefix="$(brew --prefix)"
brew_lib="$brew_prefix/lib"

collect_deps() {
  # $1 = binary/dylib path; prints absolute paths of non-system dylibs.
  # Resolves @rpath/@loader_path against the Homebrew lib dir (brew dylibs
  # are installed with @rpath and runpath @loader_path/../lib -> /opt/homebrew/lib).
  otool -L "$1" | awk 'NR>1 {print $1}' | grep -v '^/System/' | grep -v '^/usr/lib/' \
    | grep -v '^@executable_path' \
    | sed -e "s#^@rpath/#$brew_lib/#" -e "s#^@loader_path/#$brew_lib/#" | sort -u
}

# first pass: deps of the binary
declare -A seen
deps="$(collect_deps "$BIN")"
while :; do
  next=""
  for d in $deps; do
    if [[ -z "${seen[$d]:-}" ]]; then
      seen[$d]=1
      # copy to Frameworks, keeping the leaf name
      name="$(basename "$d")"
      cp -Lf "$d" "$CONTENTS/Frameworks/$name"
      for sub in $(collect_deps "$d"); do
        next="$next $sub"
      done
    fi
  done
  deps="$next"
  if [[ -z "$deps" ]]; then break; fi
done

# rewrite install names in the binary: every non-system dep -> bundled dylib
for ref in $(otool -L "$BIN" | awk 'NR>1 {print $1}' | grep -v '^/System/' | grep -v '^/usr/lib/'); do
  install_name_tool -change "$ref" "@executable_path/../Frameworks/$(basename "$ref")" \
    "$CONTENTS/MacOS/gameplayfootball-bin"
done
# rewrite the -id and dependencies *between* bundled dylibs
for d in "${!seen[@]}"; do
  fw="$CONTENTS/Frameworks/$(basename "$d")"
  install_name_tool -id "@executable_path/../Frameworks/$(basename "$d")" "$fw"
  for ref in $(otool -L "$fw" | awk 'NR>1 {print $1}' | grep -v '^/System/' | grep -v '^/usr/lib/' | grep -v '^@executable_path'); do
    install_name_tool -change "$ref" "@executable_path/../Frameworks/$(basename "$ref")" "$fw"
  done
done

# --- Info.plist ------------------------------------------------------------
cat > "$CONTENTS/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>Gameplay Football</string>
  <key>CFBundleDisplayName</key><string>Gameplay Football</string>
  <key>CFBundleIdentifier</key><string>com.gameplayfootball.game</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleExecutable</key><string>gameplayfootball</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>LSMinimumSystemVersion</key><string>12.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSPrincipalClass</key><string>NSApplication</string>
  <key>NSSupportsAutomaticGraphicsSwitching</key><true/>
</dict>
</plist>
EOF

# --- sign (ad-hoc) ---------------------------------------------------------
STAGE_DIR="$(dirname "$STAGE")"
( cd "$STAGE_DIR" && codesign --force --deep --sign - "$APP" --preserve-metadata=entitlements,flags ) 2>/dev/null \
  || ( cd "$STAGE_DIR" && codesign --force --deep --sign - "$APP" )

mkdir -p "$DIST_DIR"
ROOT="$(pwd)"
out="$DIST_DIR/GameplayFootball-v${VERSION}-macos-${ARCH}.zip"
( cd "$STAGE_DIR" && ditto -c -k --sequesterRsrc --keepParent "$APP" "$ROOT/$out" ) 2>/dev/null \
  || ( cd "$STAGE_DIR" && zip -r "$ROOT/$out" "$APP" >/dev/null )
echo "Created $out"
