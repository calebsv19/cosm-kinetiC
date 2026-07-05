#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="$PHYSICS_DIR/tmp/headless_water_mode"
SUMMARY="$OUT_DIR/run_summary.json"
PROGRESS="$OUT_DIR/run_progress.json"
RUN_DIR="$OUT_DIR/volume_frames/Water Basin"
MANIFEST="$RUN_DIR/manifest.json"
WATER_MANIFEST="$RUN_DIR/water_manifest_v1.json"

rm -rf "$OUT_DIR"
"$PHYSICS_DIR/physics_sim_headless" \
  --water-mode \
  --frames 2 \
  --sim-steps-per-frame 1 \
  --grid 16x12x8 \
  --water-level 0.42 \
  --output-root "$OUT_DIR" \
  --summary "$SUMMARY" \
  --progress "$PROGRESS" \
  --overwrite \
  --save-volume-frames

test -f "$SUMMARY"
test -f "$PROGRESS"
test -f "$MANIFEST"
test -f "$WATER_MANIFEST"
test -f "$RUN_DIR/frame_000000.vf3d"
test -f "$RUN_DIR/frame_000001.vf3d"
test -f "$RUN_DIR/frame_000000.pack"
test -f "$RUN_DIR/frame_000001.pack"
test -f "$RUN_DIR/water_surface_000000.json"
test -f "$RUN_DIR/water_surface_000001.json"
test -f "$RUN_DIR/scene_bundle.json"
test -s "$RUN_DIR/frame_000000.vf3d"
test -s "$RUN_DIR/frame_000000.pack"
test -s "$RUN_DIR/water_surface_000000.json"

rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_headless_run_summary_v1"' "$SUMMARY"
rg -q '"status"[[:space:]]*:[[:space:]]*"passed"' "$SUMMARY"
rg -q '"mode"[[:space:]]*:[[:space:]]*"water"' "$SUMMARY"
rg -q '"water_level"[[:space:]]*:[[:space:]]*0.420000' "$SUMMARY"
rg -q '"runtime_scene"[[:space:]]*:[[:space:]]*""' "$SUMMARY"
rg -q '"frames_completed"[[:space:]]*:[[:space:]]*2' "$SUMMARY"
rg -q '"save_volume_frames"[[:space:]]*:[[:space:]]*true' "$SUMMARY"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_headless_run_progress_v2"' "$PROGRESS"
rg -q '"status"[[:space:]]*:[[:space:]]*"passed"' "$PROGRESS"
rg -q '"mode"[[:space:]]*:[[:space:]]*"water"' "$PROGRESS"
rg -q '"water_level"[[:space:]]*:[[:space:]]*0.420000' "$PROGRESS"
rg -q '"manifest_version"[[:space:]]*:[[:space:]]*2' "$MANIFEST"
rg -q '"preset"[[:space:]]*:[[:space:]]*"Water Basin"' "$MANIFEST"
rg -q '"frame_contract"[[:space:]]*:[[:space:]]*"vf3d"' "$MANIFEST"
rg -q '"space_mode"[[:space:]]*:[[:space:]]*"3d"' "$MANIFEST"
rg -q '"scene_up_x"[[:space:]]*:[[:space:]]*0' "$MANIFEST"
rg -q '"scene_up_y"[[:space:]]*:[[:space:]]*1' "$MANIFEST"
rg -q '"scene_up_z"[[:space:]]*:[[:space:]]*0' "$MANIFEST"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_water_manifest_v1"' "$WATER_MANIFEST"
rg -q '"frame_contract"[[:space:]]*:[[:space:]]*"water_surface_heightfield_v1"' "$WATER_MANIFEST"
rg -q '"surface_representation"[[:space:]]*:[[:space:]]*"heightfield"' "$WATER_MANIFEST"
rg -q '"configured_water_level"[[:space:]]*:' "$WATER_MANIFEST"
rg -q '"path"[[:space:]]*:[[:space:]]*"water_surface_000000.json"' "$WATER_MANIFEST"
rg -q '"path"[[:space:]]*:[[:space:]]*"water_surface_000001.json"' "$WATER_MANIFEST"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_water_surface_heightfield_v1"' "$RUN_DIR/water_surface_000000.json"
rg -q '"layout"[[:space:]]*:[[:space:]]*"row_major_z_x"' "$RUN_DIR/water_surface_000000.json"
rg -q '"surface_axis"[[:space:]]*:[[:space:]]*"y"' "$RUN_DIR/water_surface_000000.json"
rg -q '"heights_y"[[:space:]]*:' "$RUN_DIR/water_surface_000000.json"
rg -q '"normals_xyz"[[:space:]]*:' "$RUN_DIR/water_surface_000000.json"
rg -q '"finite_normals"[[:space:]]*:[[:space:]]*true' "$RUN_DIR/water_surface_000000.json"
rg -q '"water_source"[[:space:]]*:' "$RUN_DIR/scene_bundle.json"
rg -q '"contract"[[:space:]]*:[[:space:]]*"water_manifest_v1"' "$RUN_DIR/scene_bundle.json"
rg -q '"path"[[:space:]]*:[[:space:]]*"water_manifest_v1.json"' "$RUN_DIR/scene_bundle.json"

echo "physics_sim headless water mode smoke passed: $SUMMARY"
