#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
RUNTIME_SCENE="$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_minimal.json"
OUT_DIR="$PHYSICS_DIR/tmp/headless_wind_analysis"
SUMMARY="$OUT_DIR/run_summary.json"
MANIFEST="$OUT_DIR/wind_shot_manifest.json"
TIMESERIES="$OUT_DIR/wind_analysis_timeseries.jsonl"
PROJECTION="$OUT_DIR/wind_projection_frames/frame_000003.bmp"

rm -rf "$OUT_DIR"
"$PHYSICS_DIR/physics_sim_headless" \
  --runtime-scene "$RUNTIME_SCENE" \
  --frames 4 \
  --sim-steps-per-frame 4 \
  --grid 64x32x32 \
  --wind-shot-camera side \
  --output-root "$OUT_DIR" \
  --summary "$SUMMARY" \
  --overwrite \
  --save-volume-frames \
  --save-wind-projection-frames

test -f "$SUMMARY"
test -f "$MANIFEST"
test -f "$TIMESERIES"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_headless_run_summary_v1"' "$SUMMARY"
rg -q '"status"[[:space:]]*:[[:space:]]*"passed"' "$SUMMARY"
rg -q '"wind_analysis"[[:space:]]*:' "$SUMMARY"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_wind_analysis_v1"' "$SUMMARY"
rg -q '"sampled_cells"[[:space:]]*:[[:space:]]*[1-9]' "$SUMMARY"
rg -q '"inlet_throughput"[[:space:]]*:[[:space:]]*[0-9.eE+-]*[1-9]' "$SUMMARY"
rg -q '"pressure_delta"[[:space:]]*:' "$SUMMARY"
rg -q '"drag_pressure_proxy"[[:space:]]*:' "$SUMMARY"
rg -q '"vorticity_avg"[[:space:]]*:' "$SUMMARY"
rg -q '"vorticity_max"[[:space:]]*:' "$SUMMARY"
rg -q '"save_wind_projection_frames"[[:space:]]*:[[:space:]]*true' "$SUMMARY"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_wind_shot_manifest_v1"' "$MANIFEST"
rg -q '"frames_requested"[[:space:]]*:[[:space:]]*4' "$MANIFEST"
rg -q '"sim_steps_per_frame"[[:space:]]*:[[:space:]]*4' "$MANIFEST"
rg -q '"grid_override"[[:space:]]*:[[:space:]]*true' "$MANIFEST"
rg -q '"width"[[:space:]]*:[[:space:]]*64' "$MANIFEST"
rg -q '"camera_source"[[:space:]]*:[[:space:]]*"wind_shot_profile"' "$MANIFEST"
rg -q '"camera_profile"[[:space:]]*:[[:space:]]*"side"' "$MANIFEST"
rg -q '"camera_yaw_deg"[[:space:]]*:[[:space:]]*0' "$MANIFEST"
rg -q '"camera_pitch_deg"[[:space:]]*:[[:space:]]*12' "$MANIFEST"
rg -q '"wind_analysis_timeseries"[[:space:]]*:' "$MANIFEST"
rg -q '"wind_projection_frames"[[:space:]]*:' "$MANIFEST"
rg -q '"save_wind_projection_frames"[[:space:]]*:[[:space:]]*true' "$MANIFEST"
test "$(wc -l < "$TIMESERIES" | tr -d ' ')" = "4"
rg -q '"schema":"physics_sim_wind_analysis_frame_v1"' "$TIMESERIES"
rg -q '"frame_index":3' "$TIMESERIES"
rg -q '"available":true' "$TIMESERIES"
rg -q '"pressure_delta":' "$TIMESERIES"
rg -q '"vorticity_max":' "$TIMESERIES"
test -d "$OUT_DIR/volume_frames"
test -f "$PROJECTION"
test -s "$PROJECTION"

echo "physics_sim headless wind analysis smoke passed: $SUMMARY"
