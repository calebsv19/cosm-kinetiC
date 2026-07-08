# Physics Sim Headless CLI

`physics_sim_headless` is the direct command path for running a scene project,
a retained `scene_runtime.json`, or the standalone Water Basin mode without
using the menu or mutating persistent runtime state.

For long-running detached supervision, use `physics_sim_job_runner` on top of
the same headless CLI.

Build it from a standalone GitHub clone root:

```bash
make physics_sim_headless
```

From the larger CodeWork workspace parent, use:

```bash
make -C physics_sim physics_sim_headless
```

Most examples below show CodeWork workspace-parent paths because they document
cross-program scene handoff lanes. In a standalone clone, use `./physics_sim_headless`
for the binary and keep outputs under local generated roots such as `tmp/`.

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

Run a scene project cache update:

```bash
physics_sim/physics_sim_headless \
  --scene-project /path/to/scene_project_dir \
  --frames 100 \
  --sim-steps-per-frame 8 \
  --grid 96x48x48 \
  --save-volume-frames \
  --overwrite
```

When a retained `3D` scene selected in the desktop menu is also a scene project
root, the menu reports active-cache status from
`physics_sim/active_cache_manifest.json` or `physics_sim/cache_manifest.json`.
Use `Cache Cmd` to copy the equivalent headless update command; the menu does
not execute scene-project cache updates directly.

Project mode validates that the project root exists and contains
`scene_authoring.json` plus `scene_runtime.json`; `scene_project.json` is
accepted when present. It derives the runtime scene from
`<project>/scene_runtime.json` and writes the run under
`<project>/physics_sim/runs/<run_id>`. Do not combine `--scene-project` with
`--output-root`; use direct `--runtime-scene` mode for explicit external output
roots. The run id defaults to
`physics-run-YYYYMMDDTHHMMSSZ`; set
`PHYSICS_SIM_PROJECT_CACHE_RUN_ID=physics-run-test-0001` for deterministic
tests. After a successful `--save-volume-frames` run, PhysicsSim promotes the
generated cache into `assets/vf3d/active`, `assets/physics/active`, retained
`assets/*/runs/<run_id>` slots, and relative-path cache manifests under
`physics_sim/`. It does not mutate `scene_authoring.json`.

Run a standalone Water Basin simulation:

```bash
physics_sim/physics_sim_headless \
  --water-mode \
  --frames 100 \
  --sim-steps-per-frame 4 \
  --grid 64x32x32 \
  --water-level 0.45 \
  --output-root _private_workspace_artifacts/agent_runs/physics_trio/<scene_slug>/physics_sim_water \
  --save-volume-frames
```

Useful flags:

- `--scene-project <dir>`: project-local cache-output mode. The CLI reads
  `<dir>/scene_runtime.json`, requires `<dir>/scene_authoring.json`, accepts
  optional `<dir>/scene_project.json`, and writes/promotes cache output back
  under the same project after a successful saved-volume run. This mode rejects
  `--output-root` so cache runs do not drift into disconnected artifact roots.
- `--runtime-scene <path>`: compiled retained runtime scene to project into
  PhysicsSim.
- `--water-mode`: run the standalone app-local `Water Basin` scene instead of
  requiring a retained runtime scene. This is the first Water mode scaffold:
  it seeds the lower 3D volume from `--water-level`, exports normal
  `.vf3d`/`.pack` volume frames, and writes the first render-facing water
  heightfield sidecar contract.
- `--water-level <0..1>`: normalized standalone Water Basin fill height.
  Defaults to `0.5` and is clamped to the closed interval `[0, 1]`.
- `--water-review-ripples`: apply a deterministic review disturbance to the
  exported Water mode heightfield sidecars. This does not change the underlying
  VF3D/VF3H volume state; it is a headless visual-review surface pass for
  proving RayTracing water optics before the solver grows a real wave model.
- `--water-review-ripple-amplitude <meters>`: optional metric amplitude for
  `--water-review-ripples`. Values are clamped against the export voxel size.
  Omit it to use the exporter default.
- `--water-object-fixture`: enable the deterministic
  `water_pool_submerged_solid` fixture in standalone Water mode. The fixture
  stamps a simple submerged box into the Water Basin solid-mask path and writes
  first-pass object coupling diagnostics plus export-side displacement into the
  water surface sidecars.
- `--water-pool-submerged-solid`: alias for `--water-object-fixture`.
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
- `--wind-visual-mode <mode>`: choose the renderer-free Wind fallback coloring
  used by skip-present `--save-render-frames`. Supported modes are `flow`
  (default oblique scalar blend), `speed`, `speed_deficit`, `vorticity`,
  `object_mask`, `slice_speed_deficit`, `slice_vorticity`,
  `volume_speed_deficit`, and `volume_vorticity`. The slice modes use a
  mid-depth `X/Y` diagnostic view with inlet/outlet cues, object overlays,
  moving inlet dye bands, and deterministic particle streaks keyed to frame
  index. The volume modes use the oblique tunnel projection, project moving
  inlet dye and particle streaks through depth, and draw the solid mask on top
  so the obstacle remains visible. Volume streaks are seeded across the tunnel
  and around the detected solid object, then integrated through the exported
  velocity field with a small visual cross-flow gain for readability. `flow`
  mode uses the volume overlay treatment too, so persistent inlet density/dye
  transport, object overlays, and tracer lanes can be inspected together.
  `object_mask` is the quickest way to confirm where authored static obstacles
  landed in the solver mask.
- `--overwrite`: clear an existing output root before running. Without this,
  the CLI refuses to run when the output root already exists and is not empty.
- `--resume`: reserved and currently rejected; resume needs frame-continuation
  semantics before it can be truthful.
- `--save-volume-frames`: export `.vf3d`, `.pack`, `manifest.json`, and
  `scene_bundle.json` under `volume_frames/<Preset>/`. In Water mode this also
  exports `water_manifest_v1.json` and per-frame
  `water_surface_%06d.json` heightfield sidecars. `scene_bundle.json` links the
  sidecar through `water_source` without changing the existing `fluid_source`
  VF3D contract.
- `--save-wind-projection-frames`: export deterministic headless Wind analyzer
  BMP frames under `wind_projection_frames/`. These do not require SDL/Vulkan
  renderer capture; red is max velocity magnitude, green is max density, and
  blue is max pressure magnitude through the 3D volume depth.
- `--save-render-frames`: capture rendered frames. Presented runs use the
  existing renderer. Skip-present headless Wind runs can write renderer-free
  fallback BMPs under `render_frames/` as an oblique diagnostic Wind view with
  tunnel bounds, inlet/outlet cues, selectable scalar/diagnostic Wind visual
  modes, and authored-object overlays, so they do not require SDL video
  initialization. These fallback frames are diagnostic artifacts; the planned
  user-facing direction is an in-app Wind Tunnel Inspector sourced from actual
  solver/analyzer fields.
- `--skip-present`: default. Suppresses presentation.
- `--present`: use the existing presented renderer path.

Path trust boundary:

- `physics_sim_headless` and `physics_sim_job_runner` are trusted local
  operator tools. They do not sandbox arbitrary paths and should not be exposed
  as public upload, web, or untrusted worker request endpoints without a
  separate wrapper policy.
- `--runtime-scene` is a read-only local input path. It may contain authored
  runtime scene references that are valid only on the authoring machine unless
  a later worker bundle explicitly stages those assets.
- `--scene-project` is a trusted local project directory. PhysicsSim reads
  `scene_runtime.json`, validates `scene_authoring.json`, and only writes
  PhysicsSim-owned cache/run directories and manifests under that project.
  LineDrawing-owned authoring truth is not silently modified.
- `--output-root` is the only direct run artifact root. The CLI rejects
  non-empty output roots by default; `--overwrite` is an intentional local
  operator action that recursively clears that root before writing new
  artifacts.
- `--summary` and `--progress` are sidecar write paths. Prefer leaving them
  under `--output-root` unless a supervising tool, such as the detached job
  runner, owns a separate job-status root.
- `--cancel-flag` is read-only from the headless process. The detached job
  runner owns writing `cancel_requested.flag` under the job root.
- `--jobs-root` is detached-runner state, not solver output. It owns
  `job_request.json`, `job_status.json`, `run_progress.json`, `stdout.log`,
  `stderr.log`, `pid.txt`, `cancel_requested.flag`, and
  `result_summary.json` for each job id.
- Direct detached requests may still choose their own `runtime_scene_path` and
  `output_root`. Shared job bundles rewrite artifacts under the generated job
  root; do not treat direct requests as untrusted remote payloads.
- Runtime scenes can reference runtime mesh assets by local absolute path,
  scene-relative/default mesh asset path, or supported local recovery paths.
  Those references are trusted local authoring conveniences. A portable worker
  request must stage the runtime scene, runtime mesh documents, preview
  sidecars, and run config together.
- Private/generated lanes such as `build/agent_runs/`,
  `_private_workspace_artifacts/`, `tmp/headless_*`, and detached job roots are
  run artifacts. They are evidence for agents and operators, not public source
  truth or package inputs unless a package target explicitly stages them.

Standalone Water proof:

- `make -C physics_sim test-physics-sim-headless-water-mode` runs
  `--water-mode` with a small grid, `--water-level 0.42`, and
  `--save-volume-frames`; it validates `run_summary.json`,
  `run_progress.json`, `volume_frames/Water Basin/frame_*.vf3d`, matching
  `.pack` files, `manifest.json`, `scene_bundle.json`, the Y-up space
  contract, `water_manifest_v1.json`, and per-frame water heightfield sidecars
  with finite normals.
- `make -C physics_sim test-physics-sim-headless-water-object-coupling` runs
  `--water-mode --water-object-fixture` and validates the
  `water_pool_submerged_solid` footprint, wet overlap, nonzero displaced
  volume, applied displacement delta range, and `scene_bundle.json.water_source`
  continuity. The current fixture proof reports `64` object solid cells,
  `32` wet-overlap cells, about `0.148148 m^3` displaced volume, nonzero
  displacement sample/RMS diagnostics, and object-zone slope/height-variance
  diagnostics on the smoke grid.
- `make -C physics_sim test-physics-sim-headless-water-object-quality-compare`
  runs a PhysicsSim-only WTR-6.5 comparison between a baseline
  `24x16x24` / `6` frame / `2` substep object-water fixture and a quality
  `36x18x36` / `8` frame / `3` substep fixture. It writes
  `tmp/headless_water_object_quality_compare/wtr65_quality_compare_summary.json`
  plus a text summary with sample-count, footprint, displacement RMS,
  object-zone height-variance, object-zone slope, and capped-sample deltas.
  The gate enforces bounded quality-profile object-zone roughness
  (`WTR65_MAX_OBJECT_ZONE_STDDEV_M`, default `0.010`, and
  `WTR65_MAX_OBJECT_ZONE_SLOPE`, default `0.050`) while keeping capped
  displacement samples at zero. It validates the current smoothed export-side
  displacement and deterministic wake response; it does not claim
  solver-authored wake coupling.

Water object-coupling sidecars add `summary.object_coupling` fields:

- `enabled`
- `fixture_active`
- `fixture_id`
- `object_solid_cells`
- `object_footprint_columns`
- `object_wet_overlap_cells`
- `displaced_volume_m3`
- `displacement_applied`
- `displacement_delta_min_m`
- `displacement_delta_max_m`
- `displacement_delta_sum_m`
- `displacement_delta_abs_sum_m`
- `displacement_delta_rms_m`
- `displacement_sample_count`
- `displacement_capped_sample_count`
- `displacement_weight_sum`
- `displacement_weight_max`
- `object_zone_height_min_y`
- `object_zone_height_max_y`
- `object_zone_height_avg_y`
- `object_zone_height_stddev_m`
- `object_zone_max_slope`
- `affected_min_x`
- `affected_max_x`
- `affected_min_z`
- `affected_max_z`

Single-frame Water optics proof:

- `make -C ray_tracing test-ray-tracing-render-headless-water-optics-review`
  runs `physics_sim_headless --water-mode --water-review-ripples`, warms the
  standalone basin for `18` frames at `4` solver steps per frame, selects the
  final water sidecar frame, and renders one RayTracing transparent-water BMP
  through the generated `scene_bundle.json.water_source` path.
- `make -C ray_tracing test-ray-tracing-render-headless-water-basin-surface-review`
  runs a lighter large-basin visual proof. Water mode now resolves its
  standalone default basin as a square X/Z footprint, so the final
  `water_surface_*.json` sidecar covers a broad basin surface instead of the
  earlier narrow strip. The RayTracing fixture remaps the PhysicsSim Y-up
  sidecar into its Z-up render frame, then uses deterministic review ripples
  and plain basin/floor/wall geometry for one-frame optics review.
- `make -C ray_tracing test-ray-tracing-render-headless-water-moving-light-review`
  runs the WTR-5.4 sequence proof. PhysicsSim exports a warmed Water Basin with
  deterministic review ripples, then RayTracing renders four consecutive water
  sidecar frames with an authored moving light path and verifies both
  heightfield evolution and frame-to-frame visual deltas.
- `make -C ray_tracing test-ray-tracing-render-headless-water-long-motion-review`
  runs the WTR-5.5 long-motion sparse-frame proof. PhysicsSim exports a
  `201`-frame Water Basin with `4` simulation steps per exported frame; the
  review samples frames `40, 80, 120, 160, 200` and renders full RayTracing
  basin BMP frames/contact sheets from `scene_bundle.json.water_source` under
  `ray_tracing/build/agent_runs/physics_trio/water_long_motion_review/`.

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
- Renderer-frame capture is now a hard requirement in that smoke. In
  skip-present headless Wind runs it is satisfied by the renderer-free fallback
  path, which writes an oblique `render_frames/frame_%06d.bmp` without
  initializing the SDL/Vulkan video path. The smoke also checks the render BMP
  uses the larger oblique fallback dimensions instead of silently collapsing to
  the flat analyzer projection size.
- `make -C physics_sim test-physics-sim-headless-wind-long-tunnel-video`
  runs the same fixture as a longer MP4 proof. The default `high` profile
  renders `144` `volume_vorticity` diagnostic frames at `144x36x36`, encodes
  them at `18` fps to
  `tmp/headless_wind_long_tunnel_video/wind_long_tunnel_oblique.mp4`, verifies
  that the first and final render BMP frames differ, verifies the H.264 stream
  with `ffprobe`, writes
  `tmp/headless_wind_long_tunnel_video/wind_long_tunnel_video_summary.txt`, and
  removes the transient `render_frames/` BMP directory after a successful
  encode. Set `WIND_VIDEO_KEEP_BMPS=1` to keep the intermediate BMP sequence,
  set `WIND_VIDEO_QUALITY=smoke` for a shorter `120x32x32` / `36` frame check,
  or set `WIND_VIDEO_MODE=<mode>` to compare object-mask, speed-deficit, and
  vorticity views from the same fixture.
- `make -C physics_sim test-physics-sim-headless-wind-object-comparison`
  runs three single-object long-tunnel fixtures through the renderer-free
  `volume_speed_deficit` view: blunt box, sphere (`object_type: "circle"`), and
  slim box. The default smoke profile uses `96x24x24`, `24` frames, and `2`
  solver steps per frame, then writes
  `tmp/headless_wind_object_comparison/object_comparison_summary.txt` plus
  `object_comparison_summary.json`. Set `WIND_OBJECT_COMPARISON_GRID`,
  `WIND_OBJECT_COMPARISON_FRAMES`, `WIND_OBJECT_COMPARISON_SIM_STEPS_PER_FRAME`,
  or `WIND_OBJECT_COMPARISON_MODE` to run a higher-quality comparison. The
  target validates object drag availability, positive projected area, nonblank
  changing render frames, and distinct projected-area/drag-pressure proxy
  values across the supported shapes.

Wind shot outputs:

- every headless run now initializes `wind_shot_manifest.json` under the output
  root. For `3D` Wind tunnel scenes this records the repeatable shot contract:
  runtime scene, output paths, frame count, sim steps per frame, frame export
  toggles, presentation mode, and the active analysis schema.
- every headless run also initializes `wind_analysis_timeseries.jsonl`. For
  active `3D` Wind tunnel scenes, each completed output frame appends one
  `physics_sim_wind_analysis_frame_v1` JSON object with sampled-cell count,
  inlet/outlet pressure averages, pressure delta, inlet/outlet throughput,
  throughput delta, pressure-delta drag proxy, aggregate solid-object
  stagnation-pressure/drag proxy fields, average vorticity, and max vorticity.
- Wind tunnel throughput uses role-aware signs: inlet flow is positive into the
  tunnel, outlet flow is positive out of the tunnel. Outlet throughput samples
  the configured outlet band, matching the inlet slab width, so coarse grids can
  report near-exit transport without relying on a single terminal voxel.
- Static authored 3D objects are included in the Wind tunnel solid mask. The
  carrier pass now applies a deterministic downstream wake response around an
  aggregate solid-object bounds volume, so headless stats can distinguish an
  empty tunnel from a tunnel with an object through pressure delta, throughput
  delta, and vorticity. The current left/right tunnel wake pass adds a bounded
  downstream velocity deficit and capped cross-flow swirl behind static solid
  objects; the volume diagnostic particles sample that exported cross-flow so
  object interaction is visible in MP4 output. The backend also carries inlet
  density downstream as a persistent dye/smoke scalar for left/right tunnels,
  with solid obstacles naturally creating a dye shadow. Wake velocity and
  pressure perturbations also advect downstream with decay for left/right
  tunnels, while the obstacle wake injector adds fresh perturbation near the
  object face. `volume_speed_deficit` is the clearest MP4 mode for inspecting
  this relaxation path. The analyzer now also reports an aggregate
  solid-object pressure readout for the current one-object proof lane:
  `object_drag_available`, solid-cell count, projected area,
  upstream/downstream stagnation-pressure proxy averages, signed positive
  `object_pressure_delta`, and a positive `object_drag_pressure_proxy`
  magnitude. The current left/right wake source includes an upstream
  stagnation region, downstream suction core, and time-phased lateral shedding
  lobes instead of only adding a positive pressure pocket behind the object.
  `volume_speed_deficit` now uses a wake-corridor-focused boost for the
  renderer-free volume fallback: the actual source extends farther downstream,
  and the diagnostic opacity/mark size is driven by deficit strength only in
  the detected downstream object corridor so the wake reads strongly without
  saturating the whole tunnel.
  This remains an approximate diagnostic wake model rather than full
  multidirectional scalar transport, per-object-id tables, or final
  object-surface force integration.
  `tests/fixtures/runtime_scene_wind_tunnel_3d_obstacle.json` is the current
  no-frame analyzer fixture for this path.
- `tests/fixtures/runtime_scene_wind_tunnel_3d_long_box.json` is the current
  visual proof fixture for the same path.
- `tests/fixtures/runtime_scene_wind_tunnel_3d_long_sphere.json` and
  `tests/fixtures/runtime_scene_wind_tunnel_3d_long_slim_box.json` are the
  comparison fixtures for sphere and low-blockage box behavior. A true
  arrowhead/wedge is not yet a native single-object primitive in this lane.
- non-Wind scenes may emit frame rows with `"available": false`; consumers
  should key off that field before reading Wind metrics.

Output summary schema:

```json
{
  "schema": "physics_sim_headless_run_summary_v1",
  "runtime_scene": "<input scene_runtime.json>",
  "mode": "runtime_scene",
  "output_root": "<output root>",
  "grid_override": true,
  "grid": {"width": 96, "height": 48, "depth": 48},
  "frames_requested": 2,
  "frames_completed": 2,
  "sim_steps_per_frame": 8,
  "save_volume_frames": true,
  "volume_export_start_frame": 0,
  "volume_export_stride": 1,
  "volume_export_max_frames": 0,
  "save_render_frames": false,
  "skip_present": true,
  "output_policy": "fail_if_exists",
  "result_code": 0,
  "status": "passed",
  "wind_visual_mode": "flow",
  "atmosphere": {
    "initial_state_source": "atmospheric_standalone",
    "parsed_settings_available": true,
    "settings": {
      "enabled": true,
      "seed": 240627,
      "base_density": 0.015,
      "density_scale": 15.0,
      "density_threshold": 0.30,
      "region_count": 3,
      "regions": []
    },
    "seed": {
      "seeded": true,
      "seeded_cell_count": 2880,
      "max_density": 14.56
    },
    "warm_start": {"loaded": false},
    "final_volume": {
      "debug_view_available": true,
      "active_density_cells": 1764,
      "max_density": 14.45,
      "export_cache_materialization_count": 0
    }
  }
}
```

Standalone Water summaries set `"mode": "water"` and include
`"water_level": <normalized fill height>`.

Retained-scene atmospheric summaries always include an `"atmosphere"` block so
operators can separate parse/seed/export problems without opening the VF3D by
hand. `settings` is the sanitized atmosphere payload that reached the backend,
`seed` reports the procedural initial-field result, `warm_start` reports any
loaded VF3D warm-start source, and `final_volume` reports the final debug/export
view density metrics seen by the backend report.

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
  "wind_visual_mode": "slice_vorticity",
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
  "object_drag_available": true,
  "object_solid_cells": 1815,
  "object_projected_area": 0.30250001,
  "object_upstream_pressure_avg": 72.4530563,
  "object_downstream_pressure_avg": 9.61196518,
  "object_pressure_delta": 62.8410912,
  "object_drag_pressure_proxy": 19.0094299,
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

When `--save-volume-frames` is enabled, long warm-up runs can avoid writing
every intermediate VF3D/PACK frame by selecting retained exports directly:

```bash
physics_sim/physics_sim_headless \
  --water-mode \
  --frames 1041 \
  --save-volume-frames \
  --volume-export-start-frame 150 \
  --volume-export-stride 10 \
  --volume-export-max-frames 90 \
  --output-root <output-root>
```

The selected frames keep their original simulation frame indices in filenames
and manifests. For example, start `2`, stride `2`, and max `2` writes
`frame_000002.*`, `frame_000004.*`, `water_surface_000002.json`, and
`water_surface_000004.json`; skipped frames are not emitted.

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

Detached request payload and resource boundary:

- Detached requests are trusted-local operator requests unless they arrive
  through a later worker-safe wrapper or bundle policy.
- `runtime_scene_path` must exist before submit. Direct requests may point at a
  local authored scene; portable/worker-safe requests need a bundle that stages
  the scene payload and assets explicitly.
- `output_root` is the direct artifact root for direct requests. Shared job
  bundles rewrite artifacts under the generated job root so worker-style runs
  do not write to an arbitrary direct-request output path.
- `frames` and `sim_steps_per_frame` must be positive. The direct CLI parser
  accepts broad local values for operator-run long proofs; remote or
  unattended worker wrappers should impose their own profile caps before
  submit.
- `progress_interval` must be non-negative. `0` means initial/final progress
  only.
- `grid`, when present, must be `widthxheightxdepth` with each axis in
  `4..512`.
- `volume_export_start_frame`, `volume_export_stride`, and
  `volume_export_max_frames` select which simulation frames are written when
  `save_volume_frames` is true. Defaults are start `0`, stride `1`, and max
  `0` for unlimited selected frames.
- `wind_shot_camera` / `wind_shot_camera_profile` is restricted to the known
  profiles: `three_quarter`, `side`, `top`, `downstream`, and
  `runtime_default`.
- `save_volume_frames`, `save_render_frames`, `save_wind_projection_frames`,
  `skip_present`, and `overwrite` are boolean capability flags. `overwrite`
  remains an explicit local deletion authority through the selected
  `output_root`.

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
- Keep intentional reruns under an agent/run-owned output root, not broad user
  directories, because overwrite is a local deletion authority.
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
make -C physics_sim test-physics-sim-job-runner-bundle-smoke
```

The default smoke path uses the checked-in
`tests/fixtures/runtime_scene_primitive_retained.json` retained-scene fixture,
so it can run without private machine artifacts. Set
`PHYSICS_SIM_HEADLESS_RUNTIME_SCENE=/path/to/scene_runtime.json` to rerun the
same smoke against a private local-system scene, such as a Physics Trio
gallery-room output. The smoke checks that `run_summary.json` reports completed
frames, `run_progress.json` reaches `passed`, the richer solver-step fields are
present, default output reuse is rejected, `--overwrite` reruns, and
`volume_frames/` exists. The detached runner smoke/policy/bundle lanes then
validate submit/status/cancel, overwrite safety, shared bundle projection, and
synthetic `stalled` classification.
