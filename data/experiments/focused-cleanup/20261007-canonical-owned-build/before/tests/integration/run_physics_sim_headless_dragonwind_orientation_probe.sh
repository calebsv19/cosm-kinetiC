#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
SCENE="${DRAGONWIND_ORIENTATION_PROBE_SCENE:-/Users/calebsv/Desktop/Simulations/scenes/dragon_wind/scene_runtime.json}"
OBJECT_ID="${DRAGONWIND_ORIENTATION_PROBE_OBJECT_ID:-obj3d_1}"
OUT_DIR="${DRAGONWIND_ORIENTATION_PROBE_OUT_DIR:-$PHYSICS_DIR/tmp/dragonwind_wind_orientation_probe}"
FRAMES="${DRAGONWIND_ORIENTATION_PROBE_FRAMES:-24}"
STEPS_PER_FRAME="${DRAGONWIND_ORIENTATION_PROBE_SIM_STEPS_PER_FRAME:-2}"
GRID="${DRAGONWIND_ORIENTATION_PROBE_GRID:-96x32x32}"
MODE="${DRAGONWIND_ORIENTATION_PROBE_MODE:-volume_speed_deficit}"

if [[ ! -f "$SCENE" ]]; then
  echo "DragonWind runtime scene not found: $SCENE" >&2
  echo "Set DRAGONWIND_ORIENTATION_PROBE_SCENE=/path/to/scene_runtime.json to override." >&2
  exit 1
fi

python3 "$PHYSICS_DIR/tools/wind_orientation_probe.py" \
  --runtime-scene "$SCENE" \
  --object-id "$OBJECT_ID" \
  --output-root "$OUT_DIR" \
  --headless-bin "$PHYSICS_DIR/physics_sim_headless" \
  --frames "$FRAMES" \
  --sim-steps-per-frame "$STEPS_PER_FRAME" \
  --grid "$GRID" \
  --wind-visual-mode "$MODE" \
  --wind-shot-camera side \
  --rotation-mode relative \
  --preset dragonwind-roll \
  --require-wind-tunnel
