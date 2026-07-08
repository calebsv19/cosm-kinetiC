# PhysicsSim Agent Demo Pack

This document defines the smallest source-checkout demo pack a fresh agent can
run to prove PhysicsSim headless operation.

Run the primary commands from a standalone GitHub clone root. If you are inside
the larger CodeWork workspace parent, use the listed `make -C physics_sim ...`
equivalents.

## 1. Build The Headless Binary

```bash
make physics_sim_headless
```

Workspace-parent equivalent:

```bash
make -C physics_sim physics_sim_headless
```

Expected output:

- `physics_sim/physics_sim_headless`

This proves the local source checkout can build the supported headless
entrypoint. It does not prove desktop packaging, notarization, website
downloads, worker-package install state, or remote execution.

## 2. Run The Water Smoke

```bash
make test-physics-sim-headless-water-mode
```

Workspace-parent equivalent:

```bash
make -C physics_sim test-physics-sim-headless-water-mode
```

The fixture runs:

```bash
physics_sim/physics_sim_headless \
  --water-mode \
  --frames 2 \
  --sim-steps-per-frame 1 \
  --grid 16x12x8 \
  --water-level 0.42 \
  --output-root physics_sim/tmp/headless_water_mode \
  --save-volume-frames \
  --overwrite
```

Expected files:

- `tmp/headless_water_mode/run_summary.json`
- `tmp/headless_water_mode/run_progress.json`
- `tmp/headless_water_mode/volume_frames/Water Basin/manifest.json`
- `tmp/headless_water_mode/volume_frames/Water Basin/scene_bundle.json`
- `tmp/headless_water_mode/volume_frames/Water Basin/water_manifest_v1.json`
- `tmp/headless_water_mode/volume_frames/Water Basin/water_surface_000000.json`
- `tmp/headless_water_mode/volume_frames/Water Basin/frame_000000.vf3d`
- `tmp/headless_water_mode/volume_frames/Water Basin/frame_000000.pack`

From the CodeWork workspace parent these paths are under `physics_sim/tmp/`.

Required checks:

- `run_summary.json` uses schema `physics_sim_headless_run_summary_v1`
- `run_summary.json` reports `status: "passed"`
- `run_progress.json` uses schema `physics_sim_headless_run_progress_v2`
- `run_progress.json` reports `status: "passed"`
- `manifest.json` reports `frame_contract: "vf3d"` and `space_mode: "3d"`
- `water_manifest_v1.json` reports
  `frame_contract: "water_surface_heightfield_v1"`
- `water_surface_000000.json` reports finite normals
- `scene_bundle.json` links `water_source` to `water_manifest_v1.json`

This is the shortest public source-checkout headless proof. It is
deterministic and self-contained. It proves output shape and sidecars, not
solver-authoritative two-phase water.

## 3. Run The Scene-Project Cache Fixture

```bash
make test-physics-sim-headless-scene-project-cache-output
```

Workspace-parent equivalent:

```bash
make -C physics_sim test-physics-sim-headless-scene-project-cache-output
```

Fixture inputs:

- `tests/fixtures/scene_project_cache_output_minimal/scene_project.json`
- `tests/fixtures/scene_project_cache_output_minimal/scene_authoring.json`
- `tests/fixtures/scene_project_cache_output_minimal/scene_runtime.json`

From the CodeWork workspace parent these paths are under
`physics_sim/tests/fixtures/`.

The fixture copies those inputs to a generated temp project and runs:

```bash
PHYSICS_SIM_PROJECT_CACHE_RUN_ID=physics-run-test-0001 \
physics_sim/physics_sim_headless \
  --scene-project <project-dir> \
  --frames 1 \
  --grid 8x8x8 \
  --save-volume-frames \
  --overwrite
```

Expected files:

- `<project>/physics_sim/runs/physics-run-test-0001/run_summary.json`
- `<project>/physics_sim/runs/physics-run-test-0001/run_progress.json`
- `<project>/physics_sim/runs/physics-run-test-0001/cache_manifest.json`
- `<project>/physics_sim/active_cache_manifest.json`
- `<project>/physics_sim/cache_manifest.json`
- `<project>/assets/vf3d/active/frame_000000.vf3d`
- `<project>/assets/physics/active/scene_bundle.json`

Required checks:

- active cache manifest uses schema `physics_sim_active_cache_manifest_v1`
- active run id is `physics-run-test-0001`
- active VF3D and physics directories are project-relative
- retained frame list contains frame `0`
- `scene_authoring.json` is unchanged
- `run_summary.json` records the selected project runtime scene and run root

This validates PhysicsSim-owned cache output and manifest promotion under a
scene project. It does not mutate LineDrawing-owned `scene_authoring.json`.

## 4. Optional Local Job Runner Smoke

Use only after the direct headless workflow is understood:

```bash
make test-physics-sim-job-runner-bundle-smoke
```

Workspace-parent equivalent:

```bash
make -C physics_sim test-physics-sim-job-runner-bundle-smoke
```

Expected local job files:

- `build/agent_runs/jobs/<job_id>/job_status.json`
- `build/agent_runs/jobs/<job_id>/job_request.json`
- `build/agent_runs/jobs/<job_id>/result_summary.json`
- `build/agent_runs/jobs/<job_id>/output/report.json`
- `build/agent_runs/jobs/<job_id>/output/artifacts/volume_frames/`
- `stdout.log`
- `stderr.log`

This is local trusted supervision over `physics_sim_headless`. It is not a
public upload endpoint, public worker-package download, or remote worker
submission path. There is no public remote submission API in this contract.

## Failure Triage

1. Check command stderr.
2. Check `run_summary.json`.
3. Check `run_progress.json`.
4. Check fixture-specific manifests.
5. Confirm the output root was generated under the expected temp path.

Do not switch to desktop packaging, worker packages, website deploys, registry
edits, or remote hosts to debug these first-start proofs.
