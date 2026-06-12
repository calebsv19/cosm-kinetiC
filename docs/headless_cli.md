# Physics Sim Headless CLI

`physics_sim_headless` is the direct command path for running a retained
`scene_runtime.json` without using the menu or mutating persistent runtime
state.

For long-running detached supervision, use `physics_sim_job_runner` on top of
the same headless CLI.

Build it from the CodeWork root:

```bash
make -C physics_sim physics_sim_headless
```

Run a bounded retained-scene volume simulation:

```bash
physics_sim/physics_sim_headless \
  --runtime-scene _private_workspace_artifacts/agent_runs/physics_trio/<scene_slug>/line_drawing/scene_runtime.json \
  --frames 100 \
  --sim-steps-per-frame 8 \
  --grid 96x48x48 \
  --output-root _private_workspace_artifacts/agent_runs/physics_trio/<scene_slug>/physics_sim \
  --progress-interval 25 \
  --save-volume-frames
```

Useful flags:

- `--runtime-scene <path>`: compiled retained runtime scene to project into
  PhysicsSim.
- `--frames <n>`: positive frame count.
- `--sim-steps-per-frame <n>`: run the stable core sim step `n` times before
  exporting each saved frame. Defaults to `1`. Use this to increase visible
  plume/fluid motion between output frames without inflating a single unstable
  timestep.
- `--output-root <dir>`: root for run outputs.
- `--summary <path>`: optional summary path. Defaults to
  `<output-root>/run_summary.json`.
- `--progress <path>`: optional progress path. Defaults to
  `<output-root>/run_progress.json`.
- `--progress-interval <n>`: write progress every `n` completed frames.
  Defaults to `60`; use `0` to keep only initial/final progress writes.
- `--grid <width>x<height>x<depth>`: optional 3D resolution override for
  headless analysis runs. This is useful for long Wind tunnel stats-only probes
  where the desktop default grid is too expensive for iterative solver tuning.
- `--wind-shot-camera <profile>`: deterministic retained-runtime viewport
  profile for Wind shot render captures and manifest metadata. Supported
  profiles are `three_quarter` (default), `side`, `top`, `downstream`, and
  `runtime_default`.
- `--overwrite`: clear an existing output root before running. Without this,
  the CLI refuses to run when the output root already exists and is not empty.
- `--resume`: reserved and currently rejected; resume needs frame-continuation
  semantics before it can be truthful.
- `--save-volume-frames`: export `.vf3d`, `.pack`, `manifest.json`, and
  `scene_bundle.json` under `volume_frames/<Preset>/`.
- `--save-wind-projection-frames`: export deterministic headless Wind analyzer
  BMP frames under `wind_projection_frames/`. These do not require SDL/Vulkan
  renderer capture; red is max velocity magnitude, green is max density, and
  blue is max pressure magnitude through the 3D volume depth.
- `--save-render-frames`: capture rendered frames through the existing renderer.
- `--skip-present`: default. Suppresses presentation.
- `--present`: use the existing presented renderer path.

Long-tunnel Wind visual proof:

- `tests/fixtures/runtime_scene_wind_tunnel_3d_long_box.json` is the current
  authored long-tunnel fixture. It uses a `6.0 x 1.5 x 1.5` meter scene domain,
  one locked box near the first third of the tunnel, left inlet, and right
  outlet.
- `make -C physics_sim test-physics-sim-headless-wind-long-tunnel-visual`
  runs the fixture with a `96x24x24` grid, writes Wind analyzer projection
  frames, validates the initial and final projection BMPs are nonblank and
  different, checks final Wind metrics, attempts renderer frame capture, and writes
  `tmp/headless_wind_long_tunnel_visual/long_tunnel_visual_summary.txt`.
- Renderer-frame capture in that smoke is reported separately from analyzer
  projection output. If the environment cannot initialize the SDL/Vulkan video
  path, the smoke records `renderer_blocker.txt` and keeps the deterministic
  analyzer/projection proof as the hard requirement.

Wind shot outputs:

- every headless run now initializes `wind_shot_manifest.json` under the output
  root. For `3D` Wind tunnel scenes this records the repeatable shot contract:
  runtime scene, output paths, frame count, sim steps per frame, frame export
  toggles, presentation mode, and the active analysis schema.
- every headless run also initializes `wind_analysis_timeseries.jsonl`. For
  active `3D` Wind tunnel scenes, each completed output frame appends one
  `physics_sim_wind_analysis_frame_v1` JSON object with sampled-cell count,
  inlet/outlet pressure averages, pressure delta, inlet/outlet throughput,
  throughput delta, pressure-delta drag proxy, average vorticity, and max
  vorticity.
- Wind tunnel throughput uses role-aware signs: inlet flow is positive into the
  tunnel, outlet flow is positive out of the tunnel. Outlet throughput samples
  the configured outlet band, matching the inlet slab width, so coarse grids can
  report near-exit transport without relying on a single terminal voxel.
- Static authored 3D objects are included in the Wind tunnel solid mask. The
  carrier pass now applies a deterministic downstream wake response around
  solid cells, so headless stats can distinguish an empty tunnel from a tunnel
  with an object through pressure delta, throughput delta, and vorticity.
  `tests/fixtures/runtime_scene_wind_tunnel_3d_obstacle.json` is the current
  no-frame analyzer fixture for this path.
- `tests/fixtures/runtime_scene_wind_tunnel_3d_long_box.json` is the current
  visual proof fixture for the same path.
- non-Wind scenes may emit frame rows with `"available": false`; consumers
  should key off that field before reading Wind metrics.

Output summary schema:

```json
{
  "schema": "physics_sim_headless_run_summary_v1",
  "runtime_scene": "<input scene_runtime.json>",
  "output_root": "<output root>",
  "grid_override": true,
  "grid": {"width": 96, "height": 48, "depth": 48},
  "frames_requested": 2,
  "frames_completed": 2,
  "sim_steps_per_frame": 8,
  "save_volume_frames": true,
  "save_render_frames": false,
  "skip_present": true,
  "output_policy": "fail_if_exists",
  "result_code": 0,
  "status": "passed"
}
```

Wind shot manifest schema:

```json
{
  "schema": "physics_sim_wind_shot_manifest_v1",
  "runtime_scene": "<input scene_runtime.json>",
  "output_root": "<output root>",
  "summary": "<output root>/run_summary.json",
  "progress": "<output root>/run_progress.json",
  "wind_analysis_timeseries": "<output root>/wind_analysis_timeseries.jsonl",
  "wind_projection_frames": "wind_projection_frames/frame_%06d.bmp",
  "grid_override": true,
  "grid": {"width": 64, "height": 32, "depth": 32},
  "frames_requested": 4,
  "sim_steps_per_frame": 4,
  "save_volume_frames": true,
  "save_render_frames": false,
  "save_wind_projection_frames": true,
  "skip_present": true,
  "output_policy": "overwrite",
  "camera_source": "wind_shot_profile",
  "camera_profile": "side",
  "camera_yaw_deg": 0,
  "camera_pitch_deg": 12,
  "camera_distance_scale": 1.08,
  "analysis_schema": "physics_sim_wind_analysis_frame_v1"
}
```

Wind analysis time-series row schema:

```json
{
  "schema": "physics_sim_wind_analysis_frame_v1",
  "frame_index": 0,
  "sim_steps_per_frame": 4,
  "available": true,
  "sampled_cells": 7303132,
  "pressure_delta": -0.001376565,
  "inlet_pressure_avg": -0.001376565,
  "outlet_pressure_avg": 0,
  "inlet_throughput": 0.782050848,
  "outlet_throughput": 0,
  "throughput_delta": 0.782050848,
  "drag_pressure_proxy": -0.001385713,
  "vorticity_avg": 0.070993669,
  "vorticity_max": 145.33876
}
```

Progress schema:

```json
{
  "schema": "physics_sim_headless_run_progress_v2",
  "runtime_scene": "<input scene_runtime.json>",
  "output_root": "<output root>",
  "frames_requested": 100,
  "frames_completed": 25,
  "frame_index": 25,
  "sim_steps_per_frame": 8,
  "sim_steps_completed_in_frame": 3,
  "sim_steps_total_in_frame": 8,
  "progress_ratio": 0.253750,
  "percent_complete": 25.375,
  "stage": "simulating_frame",
  "updated_at_utc": "2026-05-20T06:04:59Z",
  "status": "running"
}
```

The richer progress fields are additive and intended for long solver frames:
- `frame_index`
- `sim_steps_completed_in_frame`
- `sim_steps_total_in_frame`
- `progress_ratio`
- `stage`
- `updated_at_utc`

Detached runner:

```bash
make -C physics_sim physics-sim-job-runner

physics_sim/physics_sim_job_runner submit --request <request.json>
physics_sim/physics_sim_job_runner status --job-id <job_id>
physics_sim/physics_sim_job_runner cancel --job-id <job_id>
```

The first detached trio chain now routes through this runner via
`bin/run_trio_detached_job_chain.sh`, which performs LineDrawing authoring
synchronously, then supervises PhysicsSim and RayTracing through their file-backed
job status seams. The chain now supports named submit profiles:

- `preview`
- `review`
- `long_review`
- `overnight`

and writes both:

- `chain_status.json` for current live state
- `chain_summary.json` for top-level artifact roots, child job ids, requested
  work, and monitoring cadence guidance

The chain summary is the intended handoff point for automation policy. Codex can
read `monitoring.automation_recommendation` to decide whether to:

- poll on a fixed interval
- defer the first check until a wall-clock time like `06:00`
- then resume interval-based follow-ups

Detached request schema:

```json
{
  "schema_version": "physics_sim_headless_request_v1",
  "runtime_scene_path": "<scene_runtime.json>",
  "output_root": "<run_dir>/physics_sim",
  "frames": 100,
  "sim_steps_per_frame": 8,
  "progress_interval": 25,
  "grid": "96x48x48",
  "wind_shot_camera": "three_quarter",
  "save_volume_frames": true,
  "save_render_frames": false,
  "save_wind_projection_frames": true,
  "skip_present": true,
  "overwrite": false
}
```

Detached job status schema is `physics_sim_detached_job_status_v1` and lives
under `build/agent_runs/jobs/<job_id>/job_status.json`. It carries:
- lifecycle state: `queued`, `starting`, `running`, `stalled`, `completed`,
  `failed`, `cancelled`
- request/output paths
- pid and exit code
- frame and sim-step progress
- normalized `progress_ratio`
- submitted/started/updated/finished timestamps
- detached stdout/stderr log paths

Cancel behavior:

- `physics_sim_job_runner cancel` is cooperative first. It writes
  `cancel_requested.flag` into the job root and marks the live status as
  `cancel_requested` while the solver is still unwinding.
- `physics_sim_headless` polls that flag during the headless loop and exits with
  summary/progress `status: "canceled"` at the next safe boundary.
- This avoids relying on OS signal delivery for detached solver jobs. If the run
  already finished before the flag is observed, the status naturally reconciles
  to `completed`.

Output policy:

- Default policy is `fail_if_exists`, so agents do not accidentally mix stale
  frames with a new run.
- Use `--overwrite` when rerunning the same output root intentionally. It
  recursively clears that root before writing new artifacts.
- Resume is intentionally unsupported in this slice.

Displayless behavior:

- Skip-present volume-only runs avoid SDL video and renderer initialization.
- `--save-render-frames` and `--present` still use the existing window/Vulkan
  renderer path and may require a display-capable environment.
- Headless runs now use deterministic fixed-step advancement based on
  `physics_fixed_dt` instead of wall-clock pacing, and `--sim-steps-per-frame`
  controls how much simulated time passes between exported frames.

Validation:

```bash
make -C physics_sim test-physics-sim-headless-cli
make -C physics_sim test-physics-sim-job-runner-smoke
make -C physics_sim test-physics-sim-job-runner-policy
```

The smoke uses the Phase 2 `gallery_room_blocks_v2` LineDrawing runtime scene
and checks that `run_summary.json` reports completed frames, `run_progress.json`
reaches `passed`, the richer solver-step fields are present, default output
reuse is rejected, `--overwrite` reruns, and `volume_frames/` exists. The
detached runner smoke/policy lanes then validate submit/status/cancel,
overwrite safety, and synthetic `stalled` classification.
