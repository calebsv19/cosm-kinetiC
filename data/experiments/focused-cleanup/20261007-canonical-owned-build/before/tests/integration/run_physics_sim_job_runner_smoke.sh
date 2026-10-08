#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_ROOT="$ROOT_DIR/build/$(uname -m)"
RUNNER="$BUILD_ROOT/tools/cli/physics_sim_job_runner"
if [[ ! -x "$RUNNER" ]]; then
  RUNNER="$ROOT_DIR/build/tools/cli/physics_sim_job_runner"
fi
if [[ ! -x "$RUNNER" ]]; then
  RUNNER="$ROOT_DIR/physics_sim_job_runner"
fi

DEFAULT_RUNTIME_SCENE="$ROOT_DIR/tests/fixtures/runtime_scene_primitive_retained.json"
RUNTIME_SCENE="${PHYSICS_SIM_HEADLESS_RUNTIME_SCENE:-$DEFAULT_RUNTIME_SCENE}"
JOBS_ROOT="$ROOT_DIR/build/agent_runs/jobs"
RUN_ROOT="$ROOT_DIR/build/agent_runs/physics_sim/job_runner_smoke"
REQUEST="$RUN_ROOT/request.json"
OUTPUT_ROOT="$RUN_ROOT/output"

if [[ ! -f "$RUNTIME_SCENE" ]]; then
  echo "missing runtime scene fixture: $RUNTIME_SCENE" >&2
  echo "set PHYSICS_SIM_HEADLESS_RUNTIME_SCENE=/path/to/scene_runtime.json to override the portable fixture" >&2
  exit 1
fi

mkdir -p "$JOBS_ROOT" "$RUN_ROOT"
rm -rf "$OUTPUT_ROOT"

cat >"$REQUEST" <<EOF
{
  "schema_version": "physics_sim_headless_request_v1",
  "runtime_scene_path": "$RUNTIME_SCENE",
  "output_root": "$OUTPUT_ROOT",
  "frames": 1,
  "sim_steps_per_frame": 2,
  "progress_interval": 1,
  "save_volume_frames": true,
  "save_render_frames": false,
  "skip_present": true,
  "overwrite": false
}
EOF

SUBMIT_OUTPUT="$("$RUNNER" submit --request "$REQUEST" --jobs-root "$JOBS_ROOT")"
JOB_ID="$(printf '%s' "$SUBMIT_OUTPUT" | sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p')"

if [[ -z "$JOB_ID" ]]; then
  echo "failed to parse job id from submit output: $SUBMIT_OUTPUT" >&2
  exit 1
fi

JOB_ROOT="$JOBS_ROOT/$JOB_ID"
STATUS_FILE="$JOB_ROOT/job_status.json"
SUMMARY_FILE="$JOB_ROOT/result_summary.json"
PROGRESS_FILE="$JOB_ROOT/run_progress.json"

for _ in $(seq 1 60); do
  STATUS_JSON="$("$RUNNER" status --job-id "$JOB_ID" --jobs-root "$JOBS_ROOT")"
  if printf '%s' "$STATUS_JSON" | grep -q '"state": "completed"'; then
    break
  fi
  if printf '%s' "$STATUS_JSON" | grep -q '"state": "failed"\|"state": "cancelled"\|"state": "stalled"'; then
    echo "$STATUS_JSON" >&2
    exit 1
  fi
  sleep 1
done

STATUS_JSON="$("$RUNNER" status --job-id "$JOB_ID" --jobs-root "$JOBS_ROOT")"
if ! printf '%s' "$STATUS_JSON" | grep -q '"state": "completed"'; then
  echo "$STATUS_JSON" >&2
  echo "detached job runner smoke did not complete inside wait budget" >&2
  exit 1
fi

grep -q '"state": "completed"' "$STATUS_FILE"
grep -q '"schema_version": "physics_sim_detached_job_status_v1"' "$STATUS_FILE"
grep -q '"frames_requested": 1' "$STATUS_FILE"
grep -q '"sim_steps_per_frame": 2' "$STATUS_FILE"
grep -q '"progress_ratio": 1.000000' "$STATUS_FILE"
grep -q '"schema": "physics_sim_headless_run_summary_v1"' "$SUMMARY_FILE"
grep -q '"status": "passed"' "$SUMMARY_FILE"
grep -q '"schema": "physics_sim_headless_run_progress_v2"' "$PROGRESS_FILE"
grep -q '"status": "passed"' "$PROGRESS_FILE"
grep -q '"stage": "completed"' "$PROGRESS_FILE"
grep -q '"progress_ratio": 1.000000' "$PROGRESS_FILE"
test -f "$JOB_ROOT/stdout.log"
test -f "$JOB_ROOT/stderr.log"
test -f "$JOB_ROOT/pid.txt"
test -d "$OUTPUT_ROOT/volume_frames"

echo "physics_sim detached job runner smoke passed: $SUMMARY_FILE"
