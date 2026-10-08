#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
FIXTURE_DIR="$PHYSICS_DIR/tests/fixtures/scene_project_cache_output_minimal"
TMP_ROOT="$PHYSICS_DIR/tmp/scene_project_cache_output"
PROJECT_DIR="$TMP_ROOT/project"
RUN_ID="physics-run-test-0001"
RUN_ROOT="$PROJECT_DIR/physics_sim/runs/$RUN_ID"
ACTIVE_MANIFEST="$PROJECT_DIR/physics_sim/active_cache_manifest.json"
AUTHORING_BEFORE="$TMP_ROOT/scene_authoring.before"
AUTHORING_AFTER="$PROJECT_DIR/scene_authoring.json"

rm -rf "$TMP_ROOT"
mkdir -p "$PROJECT_DIR"
cp "$FIXTURE_DIR/scene_project.json" "$PROJECT_DIR/scene_project.json"
cp "$FIXTURE_DIR/scene_authoring.json" "$PROJECT_DIR/scene_authoring.json"
cp "$FIXTURE_DIR/scene_runtime.json" "$PROJECT_DIR/scene_runtime.json"
cp "$PROJECT_DIR/scene_authoring.json" "$AUTHORING_BEFORE"

PHYSICS_SIM_PROJECT_CACHE_RUN_ID="$RUN_ID" "$PHYSICS_DIR/physics_sim_headless" \
  --scene-project "$PROJECT_DIR" \
  --frames 1 \
  --grid 8x8x8 \
  --save-volume-frames \
  --overwrite

test -f "$RUN_ROOT/run_summary.json"
test -f "$RUN_ROOT/run_progress.json"
test -f "$RUN_ROOT/cache_manifest.json"
test -f "$ACTIVE_MANIFEST"
test -f "$PROJECT_DIR/physics_sim/cache_manifest.json"
test -d "$PROJECT_DIR/assets/vf3d/runs/$RUN_ID"
test -d "$PROJECT_DIR/assets/vf3d/active"
test -d "$PROJECT_DIR/assets/physics/runs/$RUN_ID"
test -d "$PROJECT_DIR/assets/physics/active"
test -f "$PROJECT_DIR/assets/vf3d/runs/$RUN_ID/frame_000000.vf3d"
test -f "$PROJECT_DIR/assets/vf3d/active/frame_000000.vf3d"
test -f "$PROJECT_DIR/assets/physics/runs/$RUN_ID/scene_bundle.json"
test -f "$PROJECT_DIR/assets/physics/active/scene_bundle.json"
test -s "$PROJECT_DIR/assets/vf3d/active/frame_000000.vf3d"
cmp "$AUTHORING_BEFORE" "$AUTHORING_AFTER"

rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_active_cache_manifest_v1"' "$ACTIVE_MANIFEST"
rg -q '"project_root"[[:space:]]*:[[:space:]]*"\."' "$ACTIVE_MANIFEST"
rg -q '"runtime_scene"[[:space:]]*:[[:space:]]*"scene_runtime.json"' "$ACTIVE_MANIFEST"
rg -q '"active_run_id"[[:space:]]*:[[:space:]]*"physics-run-test-0001"' "$ACTIVE_MANIFEST"
rg -q '"vf3d_active_dir"[[:space:]]*:[[:space:]]*"assets/vf3d/active"' "$ACTIVE_MANIFEST"
rg -q '"physics_active_dir"[[:space:]]*:[[:space:]]*"assets/physics/active"' "$ACTIVE_MANIFEST"
rg -q '"scene_bundle"[[:space:]]*:[[:space:]]*"assets/physics/active/scene_bundle.json"' "$ACTIVE_MANIFEST"
rg -q '"frame_count"[[:space:]]*:[[:space:]]*1' "$ACTIVE_MANIFEST"
rg -q '"retained_frame_indices"[[:space:]]*:[[:space:]]*\[[[:space:]]*0[[:space:]]*\]' "$ACTIVE_MANIFEST"
rg -q '"export_start_frame"[[:space:]]*:[[:space:]]*0' "$ACTIVE_MANIFEST"
rg -q '"export_stride"[[:space:]]*:[[:space:]]*1' "$ACTIVE_MANIFEST"
rg -q '"export_max_frames"[[:space:]]*:[[:space:]]*0' "$ACTIVE_MANIFEST"
rg -q '"created_at"[[:space:]]*:' "$ACTIVE_MANIFEST"
rg -q '"runtime_scene"[[:space:]]*:[[:space:]]*"'"$PROJECT_DIR"'/scene_runtime.json"' "$RUN_ROOT/run_summary.json"
rg -q '"output_root"[[:space:]]*:[[:space:]]*"'"$RUN_ROOT"'"' "$RUN_ROOT/run_summary.json"

echo "physics_sim headless scene project cache output passed: $ACTIVE_MANIFEST"
