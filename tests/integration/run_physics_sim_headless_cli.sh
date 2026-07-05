#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
DEFAULT_RUNTIME_SCENE="$PHYSICS_DIR/tests/fixtures/runtime_scene_primitive_retained.json"
RUNTIME_SCENE="${PHYSICS_SIM_HEADLESS_RUNTIME_SCENE:-$DEFAULT_RUNTIME_SCENE}"
OUT_DIR="$PHYSICS_DIR/tmp/headless_cli_portable_retained_scene"
SUMMARY="$OUT_DIR/run_summary.json"
PROGRESS="$OUT_DIR/run_progress.json"
STEP_LOG="/private/tmp/physics_sim_headless_step_progress.out"

if [ ! -f "$RUNTIME_SCENE" ]; then
  echo "missing runtime scene fixture: $RUNTIME_SCENE" >&2
  echo "set PHYSICS_SIM_HEADLESS_RUNTIME_SCENE=/path/to/scene_runtime.json to override the portable fixture" >&2
  exit 1
fi

rm -rf "$OUT_DIR"
"$PHYSICS_DIR/physics_sim_headless" \
  --runtime-scene "$RUNTIME_SCENE" \
  --frames 2 \
  --output-root "$OUT_DIR" \
  --summary "$SUMMARY" \
  --save-volume-frames

test -f "$SUMMARY"
test -f "$PROGRESS"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_headless_run_summary_v1"' "$SUMMARY"
rg -q '"status"[[:space:]]*:[[:space:]]*"passed"' "$SUMMARY"
rg -q '"frames_completed"[[:space:]]*:[[:space:]]*2' "$SUMMARY"
rg -q '"sim_steps_per_frame"[[:space:]]*:[[:space:]]*1' "$SUMMARY"
rg -q '"output_policy"[[:space:]]*:[[:space:]]*"fail_if_exists"' "$SUMMARY"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_headless_run_progress_v2"' "$PROGRESS"
rg -q '"status"[[:space:]]*:[[:space:]]*"passed"' "$PROGRESS"
rg -q '"frames_completed"[[:space:]]*:[[:space:]]*2' "$PROGRESS"
rg -q '"sim_steps_per_frame"[[:space:]]*:[[:space:]]*1' "$PROGRESS"
rg -q '"progress_ratio"[[:space:]]*:[[:space:]]*1.000000' "$PROGRESS"
rg -q '"sim_steps_completed_in_frame"[[:space:]]*:[[:space:]]*0' "$PROGRESS"
rg -q '"sim_steps_total_in_frame"[[:space:]]*:[[:space:]]*0' "$PROGRESS"
test -d "$OUT_DIR/volume_frames"

"$PHYSICS_DIR/physics_sim_headless" \
  --runtime-scene "$RUNTIME_SCENE" \
  --frames 1 \
  --output-root "$OUT_DIR" \
  --save-volume-frames >/tmp/physics_sim_headless_existing.out 2>&1 && {
    cat /tmp/physics_sim_headless_existing.out >&2
    echo "expected existing output root run to fail without --overwrite" >&2
    exit 1
  }
rg -q 'output root already exists and is not empty' /tmp/physics_sim_headless_existing.out
rg -q 'stage=prepare_output' /tmp/physics_sim_headless_existing.out
rg -q "output_root=$OUT_DIR" /tmp/physics_sim_headless_existing.out
rg -q 'action=choose a new output root or pass --overwrite' /tmp/physics_sim_headless_existing.out

rm -f "$STEP_LOG"
"$PHYSICS_DIR/physics_sim_headless" \
  --runtime-scene "$RUNTIME_SCENE" \
  --frames 1 \
  --sim-steps-per-frame 2 \
  --output-root "$OUT_DIR" \
  --summary "$SUMMARY" \
  --progress "$PROGRESS" \
  --progress-interval 1 \
  --overwrite \
  --save-volume-frames >"$STEP_LOG" 2>&1

rg -q '"output_policy"[[:space:]]*:[[:space:]]*"overwrite"' "$SUMMARY"
rg -q '"frames_completed"[[:space:]]*:[[:space:]]*1' "$SUMMARY"
rg -q '"sim_steps_per_frame"[[:space:]]*:[[:space:]]*2' "$SUMMARY"
rg -q '"frames_completed"[[:space:]]*:[[:space:]]*1' "$PROGRESS"
rg -q '"sim_steps_per_frame"[[:space:]]*:[[:space:]]*2' "$PROGRESS"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_headless_run_progress_v2"' "$PROGRESS"
rg -q '"stage"[[:space:]]*:[[:space:]]*"completed"' "$PROGRESS"
rg -q 'step=2/2 stage=simulating_frame' "$STEP_LOG"

echo "physics_sim headless CLI smoke passed: $SUMMARY"
