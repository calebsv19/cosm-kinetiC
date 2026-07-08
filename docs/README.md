# kinetiC Docs Index

Start here for public repository documentation.

Public identity:
- packaged desktop product: `kinetiC`
- repository/program key: `physics_sim`

Command context:
- from a standalone GitHub clone, run Make targets at the repo root, for
  example `make physics_sim_headless`
- from the larger CodeWork workspace parent, use the equivalent
  `make -C physics_sim physics_sim_headless` form shown in many internal
  verification lists below

## Scaffold State
- `docs/current_truth.md`: current runtime structure, truthful `3D` export state, and verification snapshot.
- `docs/future_intent.md`: next public direction, including the runtime mesh
  validation/authoring boundary and deferred downstream `ray_tracing` `3D`
  ingest/render handoff.
- migration-friendly verification gates:
  - `make -C physics_sim clang-build`
  - `make -C physics_sim fisics-build`
  - `make -C physics_sim dump-sema`
  - `make -C physics_sim dump-sema-runtime-3d-solver-step`
  - `make -C physics_sim dump-sema-fluid2d`
  - `make -C physics_sim dump-sema-rigid2d`
  - `make -C physics_sim dump-sema-rigid2d-collision`
  - `make -C physics_sim dump-sema-structural-runtime`
  - `make -C physics_sim dump-sema-particles2d`
  - `make -C physics_sim dump-sema-structural-solver`
  - `make -C physics_sim dump-sema-atmospheric-field`
  - `make -C physics_sim dump-sema-object-manager`
  - `make -C physics_sim dump-sema-soft-body`
  - `make -C physics_sim dump-sema-runtime-fields-2d`
  - `make -C physics_sim dump-sema-runtime-backend-2d`
  - `make -C physics_sim dump-sema-runtime-backend-3d-emitters`
  - `make -C physics_sim dump-sema-runtime-backend-3d-runtime`
  - `make -C physics_sim dump-sema-runtime-backend-3d-obstacles`
  - `make -C physics_sim dump-sema-runtime-emitter`
  - `make -C physics_sim dump-sema-runtime-obstacle`
  - `make -C physics_sim test-rigid2d-collision-contract`
  - `make -C physics_sim test-sim-runtime-backend-2d-runtime-fields-contract`
  - `make -C physics_sim test-sim-runtime-backend-2d-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-emitter-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-attached-emitter-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-obstacle-contract`
  - `make -C physics_sim test-sim-runtime-emitter-contract`
  - `make -C physics_sim test-sim-runtime-obstacle-contract`
  - `make -C physics_sim test-runtime-mesh-preview-bridge-contract`
  - `make -C physics_sim test-runtime-mesh-obstacle-proxy-contract`
  - `make -C physics_sim test-runtime-scene-bridge-contract`
  - `make -C physics_sim test-soft-body-contract`
  - `make -C physics_sim memory-check-audit`
  - `make -C physics_sim toolchain-contract`
  - `make -C physics_sim test-fast`
    - small deterministic non-GUI lane for common source changes; excludes
      headless integration, package, local-system/private-path, long
      visual/video, and remote-worker probes
  - `make -C physics_sim test-stable`
  - `make -C physics_sim run-headless-smoke`
    - currently aliases `test-stable` rather than a separate long-lived runtime lane
  - `make -C physics_sim visual-harness`
    - build-only readiness gate, not an unattended execution surface
  - `make -C physics_sim visual-artifact`
    - R6 source-run image proof; writes a validated Wind projection BMP under
      ignored `visual_artifacts/` and prints the final artifact path
  - `make -C physics_sim test-legacy`

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview, execution flow, and controls.
- `AGENTS.md` (repo root): first-read source-checkout operating contract for
  fresh human and AI agents.
- `docs/AGENT_CONTROL.md`: agent control contract, supported command ladder,
  path trust boundaries, detached local supervision, and exclusions.
- `docs/AGENT_DEMO_PACK.md`: smallest deterministic source-checkout demo pack
  with expected outputs and checks.
- `docs/KEYBINDS.md`: full keybind list across fluid and structural lanes.
- `docs/headless_cli.md`: direct `physics_sim_headless` command for
  retained runtime-scene volume runs, standalone Water Basin runs, detached
  runner usage, progress reporting, and agent-owned output-directory policy.
- `docs/wind_orientation_probe.md`: headless Wind object-orientation debug
  workflow, including the portable fixture target, the local-system DragonWind
  target, and proxy-metric interpretation.
- `docs/visual_artifact.md`: R6 source-run visual proof command, ignored
  artifact root, and output contract.
- `docs/desktop_packaging.md`: desktop app bundle workflow, launcher contract, and verification commands.
- `docs/memory_check_audit.md`: default-off fisiCs memory-check audit target,
  report paths, and current clean soft-body harness evidence.

## Current Published State
- `physics_sim` is now a truthful `3D` producer on the export side:
- public product-facing docs should treat `kinetiC` as the primary app name and
  use `physics_sim` where repo/runtime identifiers need to stay exact
  - authoritative volumetric runs emit `.vf3d` + `VF3H` `.pack`
  - planar runs stay on legacy `.vf2d` + `VFHD`
  - downstream `ray_tracing` ingest is the next separate lane, not part of this repo’s current public behavior
- atmospheric initialization is now a current app-local lane:
  - standalone Atmospheric `2D`/`3D` modes seed deterministic density and velocity fields
  - normal `3D` fluid/box presets can opt into the same procedural initializer with `Atmo Init`
  - custom presets use v14 for the optional `3D` atmospheric initial-state bit, while warm-start paths remain runtime/session config
- runtime mesh assets now have a first `3D` fluid integration:
  - shared `core_mesh_preview` sidecars provide editor/runtime visual metadata
  - actual `mesh_asset_runtime_v1` geometry is used for solver voxelization
  - imported/runtime meshes default to solid obstacles
  - closed-volume runtime mesh fills use an app-local triangle acceleration
    path with high-triangle contract coverage
  - the `3D` scaffold reuses prepared runtime mesh geometry and acceleration
    trees plus domain-specific voxel footprints through backend lifetime for
    static obstacles and mesh-attached emitters
  - runtime mesh cache entries refresh in place when the source runtime mesh
    file size/mtime signature changes
  - mesh instances can opt into the existing emitter flow as surface, heat, or
    boundary-flow emitters
  - editor role controls and Apply/Save writeback persist Solid, Visual Only,
    Surface Emitter, Surface Heat Emitter, and Boundary Flow Emitter behavior
    into each mesh object's `extensions.physics_sim`
  - runtime mesh diagnostics report mesh path, preview path, transform/bounds,
    role, obstacle/emitter footprint, acceleration stats, fallback state, bounds
    volume, and Wind projected area
- packaged desktop runs now default the shared TimerHUD overlay off, and the
  retained `3D` runtime HUD is intentionally compact; detailed mesh/cache/Wind
  diagnostics belong in inspector, headless, or CLI surfaces
- GUI status and detailed diagnostics are intentionally separate:
  - menu/status toasts stay short, user-facing, and acknowledgement-oriented
  - editor status cards may summarize Apply/Save state but should keep long
    path/cache/mesh detail in inspector or diagnostic readouts
  - launcher failures and runtime setup details belong in
    `~/Library/Logs/PhysicsSim/launcher.log` or `--print-config` /
    `--self-test`
  - direct headless failures belong in CLI stderr plus `run_summary.json`,
    `run_progress.json`, and exported diagnostic artifacts
  - detached-job failures belong in `job_status.json`, `stdout.log`,
    `stderr.log`, and wrapper stderr diagnostics
  - renderer/Vulkan initialization detail remains developer stderr unless a
    selected workflow promotes it into an inspector or artifact lane
- the retained-scene menu/editor lane is also current:
  - the right inspector draw path is app-local and now separates the current
    controls into Scene Physics, Object Physics, and Source/Emitter groups
    without changing existing button actions or save/apply behavior
  - retained `3D` Source/Jet/Sink controls live in the right Source/Emitter
    inspector context; legacy non-retained scenes keep the left global-source
    row
  - selected runtime mesh objects show cached right-inspector readouts for
    role/effect, preview metadata, diagnostic footprint, BVH/triangle stats,
    file signature state, and Wind projected area without expanding the
    runtime HUD
  - `Input Root` updates refresh the `3D` catalog immediately
  - retained-scene discovery treats the selected input root as the source of truth
  - only direct child scene directories with both `scene_authoring.json` and `scene_runtime.json` are listed
  - one grouped area layer is also supported: `<input-root>/<Area>/<Scene>/...`
  - retained-scene rows now prefer `scene_authoring.json` `scene_name` metadata over raw directory stems
  - the retained-scene `2D` editor viewport now routes fit-reset, cursor-anchor zoom, drag-pan,
    and world/screen transforms through shared `core_viewport2d`, while scene-world meaning and
    `3D` orbit behavior remain app-local
  - the editor viewport now has scene-relative far-zoom headroom for oversized retained scenes
- direct headless retained-scene runs are now current for volume output:
  - `make -C physics_sim physics_sim_headless`
  - `physics_sim/physics_sim_headless --runtime-scene <scene_runtime.json> --frames <n> --output-root <dir> --progress-interval <n> --save-volume-frames`
  - standalone Water Basin runs are also current:
    `physics_sim/physics_sim_headless --water-mode --water-level <0..1> --frames <n> --output-root <dir> --save-volume-frames`
  - runs write `run_summary.json` and `run_progress.json`
  - `run_progress.json` now exposes solver-step progress inside a frame
  - non-empty output roots are rejected unless `--overwrite` is provided
  - direct headless and detached job paths are trusted-local operator
    boundaries; `docs/headless_cli.md` owns the path trust contract for
    `runtime_scene_path`, `output_root`, `summary`, `progress`, `cancel_flag`,
    and `jobs_root`
  - skip-present volume-only runs avoid SDL video/renderer initialization
  - `--present` still requires the existing renderer path
  - skip-present Wind `--save-render-frames` runs can write nonblank
    `render_frames/` BMPs through a renderer-free diagnostic fallback with
    selectable `--wind-visual-mode` views
  - Wind long-tunnel visual proof:
    `make -C physics_sim test-physics-sim-headless-wind-long-tunnel-visual`
    validates the authored long-box tunnel fixture, nonblank and changing
    analyzer projection frames, final Wind metrics, and nonblank headless
    oblique render-frame fallback output
  - Wind long-tunnel MP4 proof:
    `make -C physics_sim test-physics-sim-headless-wind-long-tunnel-video`
    defaults to the `high` `volume_vorticity` profile, encodes fallback frames to
    `tmp/headless_wind_long_tunnel_video/wind_long_tunnel_oblique.mp4` and
    removes transient BMP frames after successful encode; the volume diagnostic
    view includes depth-projected moving inlet dye bands, particle streaks, and
    visible solid-mask obstacle overlays, and the smoke verifies first/final
    render frames differ
  - Water Basin headless proof:
    `make -C physics_sim test-physics-sim-headless-water-mode` validates
    standalone `--water-mode`, VF3D/VF3H output, Y-up manifest metadata,
    `water_manifest_v1.json`, per-frame water heightfield sidecars, and the
    `scene_bundle.json.water_source` link; standalone Water Basin now resolves
    to a square X/Z footprint for broad surface sidecars, and the RayTracing
    review fixture remaps those Y-up heightfields into its Z-up render frame
- detached supervision is now current for the same lane:
  - `make -C physics_sim physics-sim-job-runner`
  - `physics_sim/physics_sim_job_runner submit --request <request.json>`
  - `physics_sim/physics_sim_job_runner status --job-id <job_id>`
  - `physics_sim/physics_sim_job_runner cancel --job-id <job_id>`
- compiler-overlay validation now has a first explicit customer contract:
  - `make -C physics_sim clang-build`
  - `make -C physics_sim fisics-build`
  - `make -C physics_sim dump-sema`
  - `make -C physics_sim dump-sema-runtime-3d-solver-step`
  - `make -C physics_sim dump-sema-fluid2d`
  - `make -C physics_sim dump-sema-rigid2d`
  - `make -C physics_sim dump-sema-rigid2d-collision`
  - `make -C physics_sim dump-sema-structural-runtime`
  - `make -C physics_sim dump-sema-particles2d`
  - `make -C physics_sim dump-sema-structural-solver`
  - `make -C physics_sim dump-sema-atmospheric-field`
  - `make -C physics_sim dump-sema-object-manager`
  - `make -C physics_sim dump-sema-soft-body`
  - `make -C physics_sim dump-sema-runtime-fields-2d`
  - `make -C physics_sim dump-sema-runtime-backend-2d`
  - `make -C physics_sim dump-sema-runtime-backend-3d-emitters`
  - `make -C physics_sim dump-sema-runtime-backend-3d-runtime`
  - `make -C physics_sim dump-sema-runtime-backend-3d-obstacles`
  - `make -C physics_sim dump-sema-runtime-emitter`
  - `make -C physics_sim dump-sema-runtime-obstacle`
  - `make -C physics_sim test-rigid2d-collision-contract`
  - `make -C physics_sim test-sim-runtime-backend-2d-runtime-fields-contract`
  - `make -C physics_sim test-sim-runtime-backend-2d-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-emitter-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-attached-emitter-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-obstacle-contract`
  - `make -C physics_sim test-sim-runtime-emitter-contract`
  - `make -C physics_sim test-sim-runtime-obstacle-contract`
  - `make -C physics_sim test-runtime-mesh-preview-bridge-contract`
  - `make -C physics_sim test-runtime-mesh-obstacle-proxy-contract`
  - `make -C physics_sim test-runtime-scene-bridge-contract`
  - `make -C physics_sim toolchain-contract`
  - `make -C physics_sim package-linux-worker-self-test`
  - `make -C physics_sim package-linux-worker-dry-run`
  - `make -C physics_sim package-desktop`
  - `make -C physics_sim PACKAGE_TOOLCHAIN=fisics package-desktop`
  - desktop packaging stays Clang-default unless `PACKAGE_TOOLCHAIN=fisics` is set
  - Linux worker packaging now follows the Linux build-host architecture by
    default (`linux-x86_64` or `linux-aarch64`)
  - `package-linux-worker-dry-run` locally validates the worker archive
    manifests, entrypoints, selected docs/config payload, archive contents, and
    manifest-declared self-test without remote upload/install/execution
  - worker package upload/install/status/fetch is not owned by
    `package-linux-worker`; cross-host validation routes through the Linux PC
    or VPS handoff lanes, and worker-safe payloads must stage runtime scene,
    run config, runtime mesh documents, preview sidecars, and output/report
    roots together

## Runtime Persistence Policy
- tracked defaults remain under `config/`
- mutable runtime state persists under ignored `data/runtime/`

## Private Planning Docs
- Private scaffold plans/checklists are kept in the workspace private docs bucket:
  - `../../docs/private_program_docs/physics_sim/`
