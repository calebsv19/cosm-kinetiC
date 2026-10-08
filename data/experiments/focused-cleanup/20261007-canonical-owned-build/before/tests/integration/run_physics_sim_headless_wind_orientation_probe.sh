#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="${WIND_ORIENTATION_PROBE_OUT_DIR:-$PHYSICS_DIR/tmp/headless_wind_orientation_probe}"
SCENE="${WIND_ORIENTATION_PROBE_SCENE:-$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_mesh_wedge_wide.json}"
OBJECT_ID="${WIND_ORIENTATION_PROBE_OBJECT_ID:-mesh_wedge_wide}"
FRAMES="${WIND_ORIENTATION_PROBE_FRAMES:-12}"
STEPS_PER_FRAME="${WIND_ORIENTATION_PROBE_SIM_STEPS_PER_FRAME:-2}"
GRID="${WIND_ORIENTATION_PROBE_GRID:-96x24x24}"
MODE="${WIND_ORIENTATION_PROBE_MODE:-volume_speed_deficit}"

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
  --preset roll-sweep \
  --require-wind-tunnel
