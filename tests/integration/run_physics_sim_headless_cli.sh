#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
DEFAULT_RUNTIME_SCENE="$PHYSICS_DIR/tests/fixtures/runtime_scene_primitive_retained.json"
RUNTIME_SCENE="${PHYSICS_SIM_HEADLESS_RUNTIME_SCENE:-$DEFAULT_RUNTIME_SCENE}"
source "$PHYSICS_DIR/tests/integration/fixture_support.sh"
physics_fixture_supervise "$PHYSICS_DIR" "$0" "$@"
OUT_DIR="$(physics_fixture_root "$PHYSICS_DIR" headless_cli)"
SUMMARY="$OUT_DIR/run_summary.json"
PROGRESS="$OUT_DIR/run_progress.json"
STEP_LOG="$OUT_DIR/../step_progress.out"
EXISTING_LOG="$OUT_DIR/../existing_output.out"
VERSION_JSON="$("${PHYSICS_SIM_HEADLESS_BIN:-$PHYSICS_DIR/build/profiles/local-owned/bin/physics_sim_headless}" --version)"

printf '%s' "$VERSION_JSON" | python3 -c '
import json
import platform
import sys
from pathlib import Path

identity = json.load(sys.stdin)
source_root = Path(sys.argv[1])
expected_platform = {
    ("Darwin", "arm64"): "macOS-arm64",
    ("Darwin", "x86_64"): "macOS-x86_64",
    ("Linux", "aarch64"): "linux-aarch64",
    ("Linux", "x86_64"): "linux-x86_64",
}.get((platform.system(), platform.machine()), "unknown")
assert identity == {
    "program": "physics_sim",
    "worker_slug": "physics_sim_headless_worker",
    "worker_version": (source_root / "WORKER_VERSION").read_text().strip(),
    "source_program_version": (source_root / "VERSION").read_text().strip(),
    "platform": expected_platform,
}, identity
' "$PHYSICS_DIR"

if [ ! -f "$RUNTIME_SCENE" ]; then
  echo "missing runtime scene fixture: $RUNTIME_SCENE" >&2
  echo "set PHYSICS_SIM_HEADLESS_RUNTIME_SCENE=/path/to/scene_runtime.json to override the portable fixture" >&2
  exit 1
fi

"${PHYSICS_SIM_HEADLESS_BIN:-$PHYSICS_DIR/build/profiles/local-owned/bin/physics_sim_headless}" \
  --runtime-scene "$RUNTIME_SCENE" \
  --grid 8x8x8 \
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

"${PHYSICS_SIM_HEADLESS_BIN:-$PHYSICS_DIR/build/profiles/local-owned/bin/physics_sim_headless}" \
  --runtime-scene "$RUNTIME_SCENE" \
  --grid 8x8x8 \
  --frames 1 \
  --output-root "$OUT_DIR" \
  --save-volume-frames >"$EXISTING_LOG" 2>&1 && {
    cat "$EXISTING_LOG" >&2
    echo "expected existing output root run to fail without --overwrite" >&2
    exit 1
  }
rg -q 'output root already exists and is not empty' "$EXISTING_LOG"
rg -q 'stage=prepare_output' "$EXISTING_LOG"
rg -q "output_root=$OUT_DIR" "$EXISTING_LOG"
rg -q 'action=choose a new output root or pass --overwrite' "$EXISTING_LOG"

"${PHYSICS_SIM_HEADLESS_BIN:-$PHYSICS_DIR/build/profiles/local-owned/bin/physics_sim_headless}" \
  --runtime-scene "$RUNTIME_SCENE" \
  --grid 8x8x8 \
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
