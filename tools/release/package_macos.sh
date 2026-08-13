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

cp "$BIN" "$CONTENTS/MacOS/gameplayfootball"
cp -R "$BUILD_DIR/media" "$BUILD_DIR/databases" "$BUILD_DIR/football.config" "$CONTENTS/Resources/data"

# --- bundle runtime dylibs ------------------------------------------------
# Collect the dylib closure of the binary and re-point everything at the
# bundled copies (Contents/Frameworks) so the app is self-contained.

brew_prefix="$(brew --prefix)"

collect_deps() {
  # $1 = binary/dylib path; prints absolute paths of non-system dylibs
  otool -L "$1" | awk 'NR>1 {print $1}' | grep -v '^/System/' | grep -v '^/usr/lib/' \
    | grep -v '^@' | sort -u
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
      cp -L "$d" "$CONTENTS/Frameworks/$name"
      for sub in $(collect_deps "$d"); do
        next="$next $sub"
      done
    fi
  done
  deps="$next"
  if [[ -z "$deps" ]]; then break; fi
done

# rewrite install names: binary -> bundled dylibs
for d in "${!seen[@]}"; do
  install_name_tool -change "$d" "@executable_path/../Frameworks/$(basename "$d")" \
    "$CONTENTS/MacOS/gameplayfootball"
done
# rewrite dependencies *between* bundled dylibs and their own @rpath ids
for d in "${!seen[@]}"; do
  fw="$CONTENTS/Frameworks/$(basename "$d")"
  id="$(otool -D "$d" | tail -1)"
  if [[ "$id" != "$d" ]]; then
    install_name_tool -id "@executable_path/../Frameworks/$(basename "$d")" "$fw"
  fi
  for sub in $(collect_deps "$d"); do
    install_name_tool -change "$sub" "@executable_path/../Frameworks/$(basename "$sub")" "$fw"
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
codesign --force --deep --sign - "$APP" --preserve-metadata=entitlements,flags 2>/dev/null \
  || codesign --force --deep --sign - "$APP"

mkdir -p "$DIST_DIR"
out="$DIST_DIR/GameplayFootball-v${VERSION}-macos-${ARCH}.zip"
( cd "$(dirname "$STAGE")" && ditto -c -k --sequesterRsrc --keepParent "$APP" "$OLDPWD/$out" ) 2>/dev/null \
  || ( cd "$(dirname "$STAGE")" && zip -r "$OLDPWD/$out" "$APP" >/dev/null )
echo "Created $out"
