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

CODEWORK_ROOT="$(cd "$ROOT_DIR/.." && pwd)"
RUNTIME_SCENE="$CODEWORK_ROOT/_private_workspace_artifacts/agent_runs/physics_trio/gallery_room_blocks_v2/line_drawing/scene_runtime.json"
JOBS_ROOT="$ROOT_DIR/build/agent_runs/jobs"
RUN_ROOT="$ROOT_DIR/build/agent_runs/physics_sim/job_runner_policy"
REQUEST="$RUN_ROOT/request.json"
OUTPUT_ROOT="$RUN_ROOT/output"
ERR_DIR="/private/tmp/physics_sim_job_runner_policy"

if [[ ! -f "$RUNTIME_SCENE" ]]; then
  echo "missing runtime scene fixture: $RUNTIME_SCENE" >&2
  echo "run the LineDrawing gallery_room_blocks_v2 agent scene flow first" >&2
  exit 1
fi

wait_for_job() {
  local job_id="$1"
  local status_json
  for _ in $(seq 1 60); do
    status_json="$("$RUNNER" status --job-id "$job_id" --jobs-root "$JOBS_ROOT")"
    if printf '%s' "$status_json" | grep -q '"state": "completed"'; then
      return 0
    fi
    if printf '%s' "$status_json" | grep -q '"state": "failed"\|"state": "cancelled"\|"state": "stalled"'; then
      echo "$status_json" >&2
      return 1
    fi
    sleep 1
  done
  echo "timed out waiting for job $job_id" >&2
  return 1
}

submit_job() {
  local extra_args=("$@")
  local submit_output
  submit_output="$("$RUNNER" submit --request "$REQUEST" --jobs-root "$JOBS_ROOT" "${extra_args[@]}")"
  printf '%s' "$submit_output" | sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p'
}

mkdir -p "$JOBS_ROOT" "$RUN_ROOT" "$ERR_DIR"
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

JOB_ID="$(submit_job)"
[[ -n "$JOB_ID" ]]
wait_for_job "$JOB_ID"
grep -q '"overwrite_policy": "fail_if_exists"' "$JOBS_ROOT/$JOB_ID/job_status.json"
test -d "$OUTPUT_ROOT/volume_frames"

if "$RUNNER" submit --request "$REQUEST" --jobs-root "$JOBS_ROOT" >"$ERR_DIR/submit.out" 2>"$ERR_DIR/submit.err"; then
  echo "expected submit without overwrite to fail on existing output root" >&2
  exit 1
fi
grep -q 'output root already exists and is not empty; use --overwrite' "$ERR_DIR/submit.err"

JOB_ID="$(submit_job --overwrite)"
[[ -n "$JOB_ID" ]]
wait_for_job "$JOB_ID"
grep -q '"overwrite_policy": "overwrite"' "$JOBS_ROOT/$JOB_ID/job_status.json"

cat >"$REQUEST" <<EOF
{
  "schema_version": "physics_sim_headless_request_v1",
  "runtime_scene_path": "$RUNTIME_SCENE",
  "output_root": "$OUTPUT_ROOT",
  "frames": 12,
  "sim_steps_per_frame": 40,
  "progress_interval": 1,
  "save_volume_frames": false,
  "save_render_frames": false,
  "skip_present": true,
  "overwrite": true
}
EOF

JOB_ID="$(submit_job --overwrite)"
[[ -n "$JOB_ID" ]]
sleep 1
"$RUNNER" cancel --job-id "$JOB_ID" --jobs-root "$JOBS_ROOT" >/dev/null
for _ in $(seq 1 20); do
  STATUS_JSON="$("$RUNNER" status --job-id "$JOB_ID" --jobs-root "$JOBS_ROOT")"
  if printf '%s' "$STATUS_JSON" | grep -q '"state": "cancelled"'; then
    break
  fi
  sleep 1
done
STATUS_JSON="$("$RUNNER" status --job-id "$JOB_ID" --jobs-root "$JOBS_ROOT")"
printf '%s' "$STATUS_JSON" | grep -q '"state": "cancelled"'
printf '%s' "$STATUS_JSON" | grep -q '"stage": "canceled"'
grep -q '"status": "canceled"' "$JOBS_ROOT/$JOB_ID/result_summary.json"
test -f "$JOBS_ROOT/$JOB_ID/cancel_requested.flag"

FAKE_JOB_ID="psjob_fake_stalled"
FAKE_JOB_ROOT="$JOBS_ROOT/$FAKE_JOB_ID"
rm -rf "$FAKE_JOB_ROOT"
mkdir -p "$FAKE_JOB_ROOT"

sleep 30 &
FAKE_PID=$!
trap 'kill "$FAKE_PID" 2>/dev/null || true' EXIT

cat >"$FAKE_JOB_ROOT/pid.txt" <<EOF
$FAKE_PID
EOF

cat >"$FAKE_JOB_ROOT/run_progress.json" <<'EOF'
{
  "schema": "physics_sim_headless_run_progress_v2",
  "runtime_scene": "fake_scene_runtime.json",
  "output_root": "/tmp/fake_output",
  "frames_requested": 2,
  "frames_completed": 1,
  "frame_index": 1,
  "sim_steps_per_frame": 2,
  "sim_steps_completed_in_frame": 1,
  "sim_steps_total_in_frame": 2,
  "progress_ratio": 0.750000,
  "percent_complete": 75.000000,
  "stage": "simulating_frame",
  "updated_at_utc": "2026-05-20T00:00:00Z",
  "status": "running"
}
EOF

cat >"$FAKE_JOB_ROOT/job_status.json" <<EOF
{
  "schema_version": "physics_sim_detached_job_status_v1",
  "program": "physics_sim",
  "tool": "physics_sim_headless",
  "job_id": "$FAKE_JOB_ID",
  "state": "running",
  "stage": "simulating_frame",
  "request_path": "$REQUEST",
  "output_root": "$OUTPUT_ROOT",
  "progress_path": "$FAKE_JOB_ROOT/run_progress.json",
  "summary_path": "$FAKE_JOB_ROOT/result_summary.json",
  "stdout_path": "$FAKE_JOB_ROOT/stdout.log",
  "stderr_path": "$FAKE_JOB_ROOT/stderr.log",
  "pid": $FAKE_PID,
  "exit_code": -1,
  "overwrite_policy": "overwrite",
  "frames_requested": 2,
  "frames_completed": 1,
  "frame_index": 1,
  "sim_steps_per_frame": 2,
  "sim_steps_completed_in_frame": 1,
  "sim_steps_total_in_frame": 2,
  "progress_ratio": 0.750000,
  "submitted_at_utc": "2026-05-20T00:00:00Z",
  "started_at_utc": "2026-05-20T00:00:00Z",
  "finished_at_utc": "",
  "updated_at_utc": "2026-05-20T00:00:00Z",
  "diagnostics": "simulating frame"
}
EOF

STATUS_JSON="$("$RUNNER" status --job-id "$FAKE_JOB_ID" --jobs-root "$JOBS_ROOT")"
printf '%s' "$STATUS_JSON" | grep -q '"state": "stalled"'
printf '%s' "$STATUS_JSON" | grep -q '"sim_steps_completed_in_frame": 1'
printf '%s' "$STATUS_JSON" | grep -q 'no progress update for'

kill "$FAKE_PID" 2>/dev/null || true
trap - EXIT

echo "physics_sim detached job runner policy passed"
