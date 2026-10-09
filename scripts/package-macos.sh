#!/bin/bash
# Turns Godot's macOS export into the release: puts the converter inside the
# app, signs ad hoc, and zips it. Usage: scripts/package-macos.sh <export.zip>
set -euo pipefail
EXPORT_ZIP="$1"
OUT_DIR="$(dirname "$EXPORT_ZIP")"
WORK="$OUT_DIR/pkg"
rm -rf "$WORK" && mkdir -p "$WORK"
ditto -x -k "$EXPORT_ZIP" "$WORK"
APP="$(find "$WORK" -maxdepth 1 -name '*.app' | head -1)"
[ -d "$APP" ] || { echo "no .app in $EXPORT_ZIP"; exit 1; }

cp importer/target/release/ssrc-import "$APP/Contents/MacOS/ssrc-import"
chmod +x "$APP/Contents/MacOS/ssrc-import"
cp LICENSE README.md "$APP/Contents/Resources/"
cp game/fonts/LICENSE-DejaVu.txt "$APP/Contents/Resources/"

# Ad-hoc signature over the whole bundle (no paid Apple ID needed).
codesign --force --deep --sign - --timestamp=none "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
lipo -archs "$APP/Contents/MacOS/"* 2>/dev/null || true

FINAL="$OUT_DIR/Supersonic-RC-macOS-AppleSilicon.zip"
rm -f "$FINAL"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$FINAL"
echo "packaged: $FINAL"
