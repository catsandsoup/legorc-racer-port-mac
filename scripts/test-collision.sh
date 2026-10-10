#!/bin/bash
# Regression test: the Havok collision world must be solid from the side
# the player meets it (floors, walls, ramps, stairs, shelves, dinosaur).
# Needs converted data and a Godot 4.7.2 binary:
#   GODOT=/path/to/godot SSRC_DATA=/path/to/converted scripts/test-collision.sh
set -euo pipefail
: "${GODOT:?set GODOT to the Godot 4.7.2 binary}"
: "${SSRC_DATA:?set SSRC_DATA to a converted data folder}"
cd "$(dirname "$0")/.."
TMPHOME="$(mktemp -d)"
trap 'rm -rf "$TMPHOME"' EXIT
RUN=("$GODOT" --path game)
if [ "$(uname)" = Linux ]; then
  RUN=(xvfb-run -a -s "-screen 0 1280x960x24" "${RUN[@]}" --rendering-driver opengl3)
fi
OUT="$(HOME="$TMPHOME" SSRC_TEST=1 SSRC_TEST_FLOW=mode:0 SSRC_COLLISION_PROBE=1 SSRC_TEST_QUIT=30 \
  timeout 180 "${RUN[@]}" 2>&1 | grep '\[probe\]' || true)"
echo "$OUT"
echo "$OUT" | grep -q "\[probe\] ALL PASS"
