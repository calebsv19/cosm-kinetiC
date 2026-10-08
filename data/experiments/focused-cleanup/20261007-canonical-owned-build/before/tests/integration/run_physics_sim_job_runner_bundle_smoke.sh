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
RUN_ROOT="/private/tmp/physics_sim_job_runner_bundle_smoke"
FIXTURE="$ROOT_DIR/tests/fixtures/physics_sim_job_runner_bundle_request.json"
RUN_CONFIG="$RUN_ROOT/input/run.physics_sim.json"
JOB_JSON="$RUN_ROOT/job.json"
JOB_ID="ps-bundle-smoke-001"

if [[ ! -f "$RUNTIME_SCENE" ]]; then
  echo "missing runtime scene fixture: $RUNTIME_SCENE" >&2
  echo "set PHYSICS_SIM_HEADLESS_RUNTIME_SCENE=/path/to/scene_runtime.json to override the portable fixture" >&2
  exit 1
fi

rm -rf "$RUN_ROOT" "$JOBS_ROOT/$JOB_ID"
mkdir -p "$JOBS_ROOT" "$RUN_ROOT/input"

sed \
  -e "s#__RUNTIME_SCENE__#$RUNTIME_SCENE#g" \
  -e "s#__OUTPUT_ROOT__#$RUN_ROOT/ignored-inner-output#g" \
  "$FIXTURE" >"$RUN_CONFIG"

cat >"$JOB_JSON" <<EOF
{
  "schema_family": "codework_job",
  "schema_variant": "headless_bundle_v1",
  "job_id": "$JOB_ID",
  "program": "physics_sim",
  "tool": {
    "name": "physics_sim_headless",
    "version": "0.1.0",
    "target_os": "linux",
    "target_arch": "x86_64"
  },
  "scene_payload": {
    "schema_family": "codework_scene",
    "schema_variant": "scene_runtime_v1",
    "path": "$RUNTIME_SCENE"
  },
  "run_config": {
    "schema_family": "physics_sim_request",
    "schema_variant": "physics_sim_headless_request_v1",
    "path": "input/run.physics_sim.json"
  },
  "outputs": {
    "root": ".",
    "report_path": "output/report.json",
    "logs_dir": ".",
    "artifacts_dir": "output/artifacts"
  },
  "metadata": {
    "title": "Physics Sim Bundle Smoke",
    "description": "Shared outer bundle smoke for detached physics_sim jobs.",
    "created_by": "codex",
    "created_at": "2026-05-22T00:00:00Z"
  }
}
EOF

SUBMIT_OUTPUT="$("$RUNNER" submit --request "$JOB_JSON" --jobs-root "$JOBS_ROOT")"
JOB_ID="$(printf '%s' "$SUBMIT_OUTPUT" | sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p')"

if [[ "$JOB_ID" != "ps-bundle-smoke-001" ]]; then
  echo "expected bundle job id ps-bundle-smoke-001, got: $SUBMIT_OUTPUT" >&2
  exit 1
fi

JOB_ROOT="$JOBS_ROOT/$JOB_ID"
STATUS_FILE="$JOB_ROOT/job_status.json"
SUMMARY_FILE="$JOB_ROOT/result_summary.json"
SHARED_JOB_FILE="$JOB_ROOT/job.json"
SHARED_REPORT_FILE="$JOB_ROOT/output/report.json"
CANONICAL_REQUEST="$JOB_ROOT/job_request.json"
ARTIFACT_ROOT="$JOB_ROOT/output/artifacts"

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
  echo "bundle smoke did not complete inside wait budget" >&2
  exit 1
fi

grep -q '"schema_version": "physics_sim_detached_job_status_v1"' "$STATUS_FILE"
grep -q '"schema_family": "codework_job"' "$SHARED_JOB_FILE"
grep -q '"schema_variant": "headless_bundle_v1"' "$SHARED_JOB_FILE"
grep -q '"job_id": "ps-bundle-smoke-001"' "$SHARED_JOB_FILE"
grep -q '"schema_family": "codework_job_report"' "$SHARED_REPORT_FILE"
grep -q '"schema_variant": "headless_report_v1"' "$SHARED_REPORT_FILE"
grep -q '"state": "succeeded"' "$SHARED_REPORT_FILE"
grep -q '"schema": "physics_sim_headless_run_summary_v1"' "$SUMMARY_FILE"
grep -q '"output_root": "'"$ARTIFACT_ROOT"'"' "$CANONICAL_REQUEST"
test -d "$ARTIFACT_ROOT/volume_frames"

echo "physics_sim detached job runner bundle smoke passed: $SHARED_REPORT_FILE"
