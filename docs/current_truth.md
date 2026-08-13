# kinetiC Current Truth

Last updated: 2026-08-13

## Program Identity
- Repository directory: `physics_sim/`
- Public product name: `kinetiC`
- Internal/repo/runtime identifiers still use `physics_sim` and `Physics Sim`
  in launcher, log, binary, and source-level contracts where required
- Primary runtime entry:
  - `src/main.c` (`main()` -> `physics_sim_app_main(...)`)
  - wrapper shell: `include/physics_sim/physics_sim_app_main.h`, `src/app/physics_sim_app_main.c`

## Current Shipped State
- The source-grounded Vulkan lifecycle adoption is committed locally in
  `2f9bb55` (managed shared subtree refresh) and `e534f95` (kinetiC integration),
  against canonical shared commit
  `60084f90564105983c7c74e862a299d8b6775347`: the managed subtree now carries
  `vk_runtime 0.6.0` and `vk_renderer 1.3.1`, and the existing
  `VkRendererDevice` singleton delegates instance/device/queue ownership to
  `VkRuntime`. The app-local `vulkan-rollout-self-test` proves shared-device
  handle identity, validation-clean startup/resize/restart, deterministic BMP
  capture readback, and 2.0x Retina drawable sizing (`1440x900` then
  `1800x1120`). This is committed source truth, but not a version bump,
  release, publication, Registry promotion, Linux package proof, or
  compute-kernel adoption. The simulation and CPU solver paths remain app-owned
  and unchanged.
- Producer-side truthful `3D` export is complete (through `PSBU-11D`).
- The first external-agent source-checkout contract is documented in
  `AGENTS.md`, `docs/AGENT_CONTROL.md`, and `docs/AGENT_DEMO_PACK.md`. It
  starts with `physics_sim_headless`, the deterministic Water smoke, and the
  scene-project cache-output fixture. It does not claim desktop package
  freshness, website download validation, remote worker submission, registry
  mutation, release publication, public worker-package downloads, or a public
  remote submission API. The agent docs show both standalone GitHub clone
  commands and CodeWork workspace-parent `make -C physics_sim ...` forms.
- Public website agent discovery is live through
  `https://ecosystem.calebsv.tech/agents/index.json`,
  `https://ecosystem.calebsv.tech/agents/programs/cosm-kinetic.json`, and
  `https://ecosystem.calebsv.tech/agents/programs/cosm-kinetic.md`. Those
  public files are the supported external discovery layer; private registry and
  release-control evidence is maintainer-only.
- Direct retained-scene headless CLI volume runs are available through
  `physics_sim_headless`.
- Headless scene-project cache output is now available through
  `physics_sim_headless --scene-project <dir> --save-volume-frames`. Project
  mode validates `scene_authoring.json` and `scene_runtime.json`, accepts
  optional `scene_project.json`, derives the runtime scene from the project,
  writes the default run under `physics_sim/runs/<physics-run-id>`, promotes
  VF3D/physics artifacts into `assets/vf3d/active` and
  `assets/physics/active`, and writes project-relative cache manifests without
  mutating `scene_authoring.json`.
- Standalone Water Basin headless runs are available through
  `physics_sim_headless --water-mode`; the current scaffold seeds the interior
  bottom 3D volume from a normalized `water_level`, resolves the default basin
  as a square X/Z footprint, exports standard VF3D/VF3H frames, and writes
  explicit water heightfield sidecars. The optional
  `--water-object-fixture` path now stamps the deterministic
  `water_pool_submerged_solid` fixture into the Water Basin solid mask and
  writes object-coupling/displacement diagnostics into water sidecars.
- Detached submit/status/cancel supervision is now available through
  `physics_sim_job_runner`.
- The first trio detached chain adapter now routes through
  `bin/run_trio_detached_job_chain.sh`, which performs LineDrawing authoring
  synchronously, submits PhysicsSim as the first detached child, and leaves
  RayTracing submission to a later status refresh after `scene_bundle.json`
  becomes available.
- `physics_sim_headless` now writes both `run_summary.json` and
  `run_progress.json`, rejects non-empty output roots by default, and requires
  `--overwrite` for intentional reruns into the same root. `--resume` is
  reserved and currently rejected.
- `run_summary.json` now includes an `atmosphere` diagnostics block for
  retained-scene/headless runs. It records the backend initial-state source,
  sanitized parsed atmosphere settings, procedural seed cell count/max density,
  VF3D warm-start stats when applicable, and final debug/export-facing volume
  density metrics.
- Headless volume export can now select retained frames natively with
  `--volume-export-start-frame`, `--volume-export-stride`, and
  `--volume-export-max-frames`; summary/progress JSON report selected and
  skipped volume frame counts, and detached job-runner requests preserve the
  same fields.
- Headless and detached job paths are trusted-local operator boundaries, not
  public upload or untrusted worker request surfaces. `--runtime-scene` is a
  read-only local input, `--output-root` is the direct run artifact root,
  `--summary`/`--progress` are sidecar writes, `--cancel-flag` is read by
  headless and written by the detached runner under the job root, and
  `--jobs-root` owns detached job state. Direct detached requests may choose
  local scene/output paths; shared job bundles rewrite artifacts under the job
  root.
- `run_progress.json` is now solver-step aware:
  - schema `physics_sim_headless_run_progress_v2`
  - per-frame `sim_steps_completed_in_frame`
  - `sim_steps_total_in_frame`
  - normalized `progress_ratio`
  - progress `stage`
  - `updated_at_utc`
- Detached PhysicsSim jobs now write:
  - `build/agent_runs/jobs/<job_id>/job_request.json`
  - `job_status.json`
  - `run_progress.json`
  - `stdout.log`
  - `stderr.log`
  - `pid.txt`
  - `result_summary.json`
  - output-root artifacts under the requested run directory
- The packaged desktop launcher defaults the shared TimerHUD overlay off for
  normal app use. Developers can still re-enable it with
  `PHYSICS_SIM_TIMER_HUD=1` / `PHYSICS_SIM_TIMER_HUD_OVERLAY=1`.
- A private Linux GUI desktop package proof lane now exists for the windowed
  app. `package-linux-desktop-determinism-test` builds a `desktop_app_linux`
  archive on Linux x86_64, verifies a deterministic `.tar.gz` plus `.sha256`
  sidecar, and runs an unpacked launcher `--self-test`. Linux PC proof
  `pslgui4-20260709a` built
  `kinetiC-0.3.0-linux-x86_64-desktop-stable.tar.gz`, verified checksum
  `ebd581a014abfe69f02df8745ca8bf645fea2b3a7551065ce59aeb47683a572e`,
  launched the unpacked package in the logged-in X11 desktop session
  (`DISPLAY=:0`, `XAUTHORITY=/tmp/xauth_SmfAha`), and captured nonblank
  app-window screenshots. This remains private proof capability only: no
  `VERSION` bump, public release artifact, website metadata, production
  registry state, or worker-package install changed.
- Detached status schema is `physics_sim_detached_job_status_v1` and exposes
  `queued`, `starting`, `running`, `stalled`, `completed`, `failed`, and
  `cancelled` states without requiring a live PTY.
- Authoritative volumetric (`XYZ`) runs now emit:
  - raw `.vf3d`
  - additive `VF3H` `.pack`
  - truthful `manifest.json` / `scene_bundle.json` metadata (`frame_contract=vf3d`, `space_mode=3d`, `axis_authority=xyz`)
- Water mode keeps the existing 3D density/velocity/pressure/solid-mask fields
  for VF3D/VF3H debugging and now additionally emits
  `water_manifest_v1.json` plus per-frame `water_surface_%06d.json`
  heightfield sidecars. The sidecar uses X/Z row-major samples, Y heights,
  finite normals, surface min/max/average diagnostics, wet/dry/solid column
  counts, material defaults for later RayTracing water import, and optional
  `summary.object_coupling` diagnostics for the `water_pool_submerged_solid`
  fixture: object solid cells, footprint columns, wet-overlap cells,
  approximate displaced volume, affected sample bounds, and applied
  displacement delta range. WTR-6.5 adds object-local quality diagnostics to
  the same sidecar contract: displacement sample count, kernel weight
  sum/max, delta sum/absolute-sum/RMS, capped-sample count, object-zone height
  min/max/average/stddev, and object-zone max slope. These are reporting
  metrics over the current export-side response, not solver-authored wake
  coupling yet. `test-physics-sim-headless-water-object-quality-compare` now
  runs a PhysicsSim-only baseline-vs-quality comparison and writes
  `wtr65_quality_compare_summary.json`. The current WTR-6.5 export-side
  response uses broader/lower displacement support plus a bounded
  deterministic ring/shear wake term; wet-stencil object-zone diagnostics now
  show low roughness (`0.003482646 m` quality-profile height stddev and
  `0.019251581` quality-profile max slope in the default comparison), with
  zero capped displacement samples. This remains an export-side approximation,
  not solver-authored two-phase water.
- First-pass parity fixture is locked and deterministic (tiny-domain proof lane).
- Downstream consumer work remains separate:
  - `ray_tracing` now ingests Water Basin `scene_bundle.json.water_source`
    sidecars for backend/headless transparent-water review and remaps
    PhysicsSim Y-up heights into RayTracing's Z-up render frame for horizontal
    basin views; WTR-5.4 moving-light multi-frame review is landed, WTR-5.5
    adds a long-motion sparse full-RayTracing basin review from frames
    `40, 80, 120, 160, 200` of a `201`-frame Water Basin run, and WTR-6 now
    has a first object-water proof with deterministic displacement diagnostics,
    a full-RayTracing basin frame sequence/MP4 review under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_review/`,
    a WTR-6.5 direct-light smoothed-wake preview under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_wtr65_direct_light_preview/`,
    a matched short WTR-6.5 Disney-v2 temporal-2 comparison under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_wtr65_disney_v2_t2_short_compare/`,
    and a corrected local `100`-frame Disney-v2 slow-light review under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_hq_local_64/ray_tracing_disney_v2_local_t2_100f_slowlight_corrected/`;
    stronger solver-authored object coupling, smoother object-water boundary
    behavior, and editor controls remain follow-up cross-program boundaries.
- Atmospheric preset initialization is now available as an app-local current-state lane:
  - standalone Atmospheric `2D` and `3D` modes seed deterministic density and velocity fields from the atmospheric sampler instead of starting blank
  - normal `3D` fluid/box presets can opt into the same procedural initializer through the compact `Atmo Init` settings control while keeping their normal mode identity
  - custom preset persistence uses v14 for the optional `3D` atmospheric initial-state bit alongside embedded `ATMOS` settings; older v13 files load with that optional layer off
  - exported Atmospheric `3D` `.vf3d` / `VF3H` `.pack` frames can be selected as session-local warm starts for Atmospheric `3D`; warm-start file paths are runtime config, not portable preset-owned data
  - runtime reports and the HUD distinguish blank starts, standalone atmospheric procedural starts, optional atmospheric procedural starts, and loaded warm-start starts
- Runtime mesh assets now have a first actual-geometry `3D` fluid integration:
  - retained runtime-scene mesh instances can carry shared `core_mesh_preview`
    sidecar path/probe/metadata for editor and runtime overlay display
  - preview sidecars are visual and diagnostic payloads only; PhysicsSim solver
    effects use authoritative `mesh_asset_runtime_v1` geometry
  - imported/runtime mesh assets default to solid obstacles when a runtime mesh
    path is available
  - mesh instances can opt out with `visual_only`, `none`, or
    `fluid_obstacle: false`
  - mesh instances can opt into the existing emitter flow with
    `extensions.physics_sim.fluid_behavior` values such as `surface_emitter`,
    `surface_heat_emitter`, and `boundary_flow_emitter`, or the structured
    `extensions.physics_sim.emitter` object
  - mesh emitters clear solid obstacle occupancy and emit density, velocity,
    sink, and heat through voxelized actual runtime mesh footprints
  - closed-volume runtime mesh fills now use a first app-local triangle
    acceleration path over transformed runtime mesh triangles, and high-triangle
    generated contract coverage proves acceleration stats for imported-mesh-like
    assets
  - the `3D` scaffold backend keeps a prepared runtime mesh cache for loaded
    documents, transformed vertices, bounds, and acceleration trees so static
    mesh obstacles and mesh-attached emitters can reuse mesh work across backend
    lifetime
  - prepared runtime mesh entries also cache domain-specific voxel footprints,
    allowing repeated static fixtures to stamp occupied cell indices without
    rerunning triangle shell rasterization and closed-volume point tests on
    every pass
  - prepared runtime mesh cache entries include runtime file size/mtime
    signatures, so edited mesh files trigger an in-place stale refresh on the
    next cache lookup
  - oversized mesh/grid intersections use a conservative actual-mesh
    bounds-fill fallback instead of stalling the solver
  - the scene editor exposes selected runtime mesh role controls for Solid,
    Visual Only, Surface Emitter, Surface Heat Emitter, and Boundary Flow
    Emitter
  - selected runtime mesh objects now show a compact right-inspector readout
    for solver role/effect, runtime file, preview metadata/probe state,
    cached diagnostic footprint, runtime triangle and BVH stats, runtime file
    signature availability, and Wind projected area; this readout is cached on
    editor state and is not drawn through the always-on HUD
  - runtime mesh/import diagnostics are split by owner: import bridges report
    authored/runtime-scene input and path-resolution problems; runtime mesh
    diagnostics report role, preview/runtime paths, voxel footprint, BVH/cache
    fallback, bounds/domain overlap, and Wind projected area; wording-only
    changes should not modify voxelization, prepared-cache behavior, live-watch
    behavior, or Wind inspector UI
  - runtime scene/runtime mesh paths are trusted local authored inputs unless a
    worker-safe bundle stages the scene, runtime mesh documents, preview
    sidecars, and run config together; absolute paths and `$HOME/Desktop/stls`
    recovery are local-authoring conveniences, not portable payload guarantees
  - Apply/Save persists per-mesh role and emitter parameters into each mesh
    object's `extensions.physics_sim` block while preserving non-PhysicsSim
    object extensions
  - the runtime scene emitter diagnostics tool reports each mesh instance's
    runtime path, preview path, transform, bounds, role, voxel footprints,
    acceleration usage/tree stats, budget fallback state, bounds volume, and
    Wind projected area

## Runtime and Editor Snapshot
- Runtime/editor retained-scene lanes are active and structurally separated from legacy compatibility mapping.
- Retained `3D` runtime views use a compact top-left HUD summary for run state,
  domain, volume, slice, Wind metrics, and quality. Detailed backend, mesh,
  cache, and Wind object diagnostics should route through inspector panels,
  headless outputs, or diagnostic CLIs instead of the always-on HUD.
- GUI status and detailed diagnostics remain separate surfaces. Menu status
  text is for short user-facing confirmations or acknowledgement prompts;
  editor status cards summarize Apply/Save state; inspectors own selected
  object/runtime-mesh/Wind details; launcher setup detail belongs in
  `launcher.log`, `--print-config`, and `--self-test`; direct headless detail
  belongs in stderr plus `run_summary.json`, `run_progress.json`, and exported
  artifacts; detached jobs own `job_status.json`, `stdout.log`, `stderr.log`,
  and wrapper stderr diagnostics; renderer/Vulkan stderr remains developer
  diagnostics unless a later selected workflow promotes it into a user-facing
  inspector or artifact lane.
- The retained-scene editor right pane now routes through an app-local
  inspector module with explicit Scene Physics, Object Physics, and
  Source/Emitter grouping. Retained `3D` Source/Jet/Sink controls now live in
  that right Source/Emitter inspector context, while legacy non-retained scenes
  keep the left-pane global source row. Selected runtime mesh objects also show
  cached Object Physics readouts for mesh role/effect, preview metadata,
  diagnostic voxel/BVH stats, file signature state, and Wind projected area.
  Existing button actions and save/apply behavior are unchanged.
- Menu/editor shell buttons now use bounded shared `kit_ui` button
  spec/state/style semantics through app-local `physics_sim_ui_button.*`
  wrappers, while SDL drawing, palette tuning, and button placement remain
  app-local.
- Agent/headless retained-scene runs can now bypass menu interaction:
  - standalone clone build: `make physics_sim_headless`
  - workspace-parent build: `make -C physics_sim physics_sim_headless`
  - standalone clone scene-project run:
    `./physics_sim_headless --scene-project <project_dir> --frames <n> --grid <w>x<h>x<d> --save-volume-frames --overwrite`
  - workspace-parent scene-project run:
    `physics_sim/physics_sim_headless --scene-project <project_dir> --frames <n> --grid <w>x<h>x<d> --save-volume-frames --overwrite`
- Retained `3D` menu selection now recognizes selected scene-project roots
  that contain `scene_authoring.json` and `scene_runtime.json`, reports concise
  project cache state from `physics_sim/active_cache_manifest.json` or the
  compatibility `physics_sim/cache_manifest.json`, and exposes a
  `Copy Cache Cmd` affordance that copies the matching
  `physics_sim/physics_sim_headless --scene-project ... --save-volume-frames --overwrite`
  update command. This is a read-only guidance surface; it does not run the
  cache update from the GUI. The menu now renders contextual Data I/O:
  legacy/direct selections keep editable `Output Root` and `Input Root` rows,
  while scene-project selections show a `Scene Project Cache` panel with
  read-only `Project Root`, artifact-aware `Cache Target`, and active-run
  rows plus `Frames`, `Headless`, and `Copy Cache Cmd`. The cache target
  readout now distinguishes no-cache, ready active VF3D/physics bundle, and
  manifest-present-but-missing-artifact states by checking the manifest's
  active VF3D directory, physics directory, and scene bundle path. In
  scene-project mode, the selected project root is the input/output container;
  legacy roots remain in config but are not shown as cache destinations. The
  bottom action row remains only `Duplicate`, `Edit Preset`, and `Start`.
  - standalone clone runtime-scene run:
    `./physics_sim_headless --runtime-scene <scene_runtime.json> --frames <n> --output-root <dir> --progress-interval <n> --save-volume-frames`
  - workspace-parent runtime-scene run:
    `physics_sim/physics_sim_headless --runtime-scene <scene_runtime.json> --frames <n> --output-root <dir> --progress-interval <n> --save-volume-frames`
  - or run the standalone basin with
    `./physics_sim_headless --water-mode --water-level <0..1> --frames <n> --output-root <dir> --save-volume-frames`
  - pass `--volume-export-start-frame <n> --volume-export-stride <n>
    --volume-export-max-frames <n>` to write only selected retained VF3D/PACK
    and water-surface sidecar frames during long warm-up runs
  - output includes `run_summary.json` under the selected output root
  - `run_progress.json` now advances inside a frame instead of only at frame boundaries
  - volume exports keep the existing `volume_frames/<Preset>/` layout with
    `.vf3d`, `.pack`, `manifest.json`, and `scene_bundle.json`
  - standalone Water mode also writes `water_manifest_v1.json` and
    `water_surface_%06d.json`; `scene_bundle.json.water_source` points to the
    water manifest while `scene_bundle.json.fluid_source` remains the VF3D
    volume contract
  - `--water-object-fixture` / `--water-pool-submerged-solid` enables the
    deterministic object-in-water fixture for backend review; the first
    response is an export-side displacement approximation over the existing
    solid-mask path, not a full two-phase CFD solver
  - skip-present volume-only runs avoid SDL video and renderer initialization
  - presented runs still use the existing window/Vulkan renderer path
  - skip-present Wind `--save-render-frames` runs can write nonblank
    `render_frames/` BMPs through a renderer-free diagnostic fallback. The
    fallback supports `--wind-visual-mode` views for oblique flow/speed,
    speed deficit, vorticity, object mask, mid-depth speed deficit, and
    mid-depth vorticity, plus oblique volume speed-deficit/vorticity views
    with depth-projected inlet dye, particle streaks, and visible solid-mask
    obstacle overlays. These fallback views are diagnostic artifacts; the
    product direction is an in-app Wind Tunnel Inspector sourced from actual
    solver/analyzer fields.
  - Wind long-tunnel visual proof is available with
    `make -C physics_sim test-physics-sim-headless-wind-long-tunnel-visual`;
    it validates the long-box tunnel fixture, nonblank and changing analyzer
    projection BMPs, final Wind metrics, and nonblank headless render-frame
    fallback output without SDL video initialization
  - Wind long-tunnel MP4 proof is available with
    `make -C physics_sim test-physics-sim-headless-wind-long-tunnel-video`;
    it defaults to the `volume_vorticity` diagnostic view in the `high`
    quality profile, encodes fallback frames to
    `tmp/headless_wind_long_tunnel_video/wind_long_tunnel_oblique.mp4` and
    removes transient BMP frames after successful encode. The volume diagnostic
    view is still a renderer-free software visualization, but it now reads as a
    3D tunnel: the obstacle is visible in depth, and the moving tracer/dye cues
    are distributed through the inlet volume. The tracer population now includes
    object-adjacent seeds, and volume streaks integrate backward through the
    exported `velocity_x/y/z` field with modest cross-flow visual gain so the
    wake is easier to inspect in low-resolution smoke videos. The video smoke
    fails if the first and final render BMPs are identical. The Wind backend
    now advects inlet density as a persistent dye/smoke scalar along left/right
    tunnels, so `flow` mode can show a solver-owned plume and obstacle shadow
    rather than only cosmetic inlet bands. The current scalar transport is still
    an axis-aligned first pass, and the solver state still reaches a mostly
    steady field rather than a long, turbulent wake trail, so multidirectional
    scalar advection and true wake relaxation/advection remain the next solver
    boundary. Wake velocity/pressure perturbations now also advect downstream
    with decay in left/right tunnels instead of being erased by each baseline
    corridor write. The obstacle wake injector is now a near-object source, so
    downstream deficit structure is carried by the solver state rather than
    repainted across the full tunnel every frame. `volume_speed_deficit` is the
    clearer visual proof for this behavior; aggregate vorticity metrics are
    still dominated by the near-object source region. The Wind analyzer now
    reports a one-object proof readout derived from the aggregate solid-mask
    bounds: object drag availability, solid-cell count, projected area,
    upstream/downstream stagnation-pressure proxy averages, signed positive
    object pressure delta, and positive object drag-pressure proxy magnitude.
    The left/right wake source now uses an upstream stagnation region, a
    downstream suction core, and time-phased lateral vortex-shedding lobes
    instead of only adding positive downstream pressure. This is intentionally
    still an aggregate one-obstacle proxy, not a per-object-id table or final
    surface-force integration. The `volume_speed_deficit` fallback now also
    boosts only the detected downstream object wake corridor, using
    deficit-driven opacity/mark size and a longer backend source so the wake
    shading is visibly stronger without saturating the whole tunnel.
  - Wind object comparison proof is available with
    `make -C physics_sim test-physics-sim-headless-wind-object-comparison`.
    It runs the long-tunnel box, sphere (`object_type: "circle"`), and slim-box
    fixtures through the same renderer-free `volume_speed_deficit` view, writes
    `tmp/headless_wind_object_comparison/object_comparison_summary.txt`, and
    validates nonblank/changing render frames plus distinct projected-area and
    drag-pressure proxy values. This is still a supported-primitive comparison:
    true arrowhead/wedge behavior requires a native single-object primitive or
    per-object force/readout IDs before it can be reported cleanly.
  - The next `3D` Wind product boundary is not more MP4 tuning. It is a
    user-facing Wind Tunnel Inspector with field slices, streamlines/pathlines,
    inlet/outlet labels, object readout, and metrics sampled from the live
    solver/analyzer state.
  - The Wind backend now applies a bounded obstacle wake pass after the uniform
    corridor throughflow. For left/right tunnels, the pass derives one aggregate
    solid-object bounds volume, then applies a downstream velocity deficit,
    capped cross-flow swirl, and pressure variation in the exported velocity
    field behind that object. The volume particle diagnostic
    samples those cross-flow components, so tracers visibly bend around the
    obstacle instead of only moving straight through the tunnel. This is still
    an approximate wake model, not full CFD turbulence or object-surface force
    integration.
- Detached agent supervision can now bypass a live terminal:
  - build with `make -C physics_sim physics-sim-job-runner`
  - submit `physics_sim/physics_sim_job_runner submit --request <request.json>`
  - inspect `physics_sim/physics_sim_job_runner status --job-id <job_id>`
  - cancel `physics_sim/physics_sim_job_runner cancel --job-id <job_id>`
  - or let `bin/run_trio_detached_job_chain.sh` submit PhysicsSim on behalf of
    a chained LineDrawing -> PhysicsSim -> RayTracing run root
- Dense retained-runtime `3D` domains now enforce a resident-memory budget before backend allocation:
  - oversized volumetric domains are downscaled by increasing voxel size rather than attempting full dense allocation at the requested resolution
- The `3D` scaffold backend no longer requires full dense field residency as its always-live source of truth:
  - persistent fluid state is stored in app-local sparse bricks
  - solver steps materialize bounded dense work regions around active bricks instead of solving over the whole domain every frame
  - obstacle occupancy is tracked at both cell and brick scope so enforcement and solver-region expansion can skip fully empty brick lanes
  - solver-region scheduling is brick-aligned and pulls in nearby obstacle bricks instead of relying on whole-volume obstacle scans
  - compatibility slices and export/debug volume views are derived readouts rather than the authoritative storage path
  - a dense mirror is kept only for bounded test-scale domains so existing small-domain contract lanes remain inspectable without reintroducing high-end dense residency
  - live `3D` stepping is sparse-authoritative even when the bounded dense mirror exists; the step path no longer resyncs sparse truth from dense arrays before solve
  - backend reporting now exposes sparse-runtime region metrics, dense-mirror-live state, and an explicit solver-region cell budget/guard result
  - oversized sparse solver regions now stop at an explicit guardrail instead of silently rematerializing arbitrarily large dense work buffers
  - live `3D` stepping now schedules connected active-brick clusters instead of one global padded region:
    - distant active plumes solve independently when their padded solver regions do not overlap
    - overlapping padded regions are merged before solve so nearby activity still behaves as one safe cluster
    - backend reporting now exposes solver-cluster count, max per-cluster solver cell count, and cluster-limit fallback state
  - obstacle authority beneath the sparse runtime is now brick-local first:
    - live obstacle writes, rebuilds, solver-region solid-mask materialization, and obstacle enforcement use per-brick occupancy masks
    - the dense whole-domain obstacle mask is now a derived compatibility/export/readout cache instead of the live control plane
    - debug/export/test surfaces can explicitly zero the dense obstacle cache and still recover truthful obstacle state from the brick-local masks
  - support-surface cache paths are now materially sparser:
    - full-volume export cache rebuilds now materialize only allocated fluid bricks and occupied obstacle bricks instead of rescanning the full domain cell-by-cell
    - sparse debug-volume stats now derive from active fluid cells and occupied obstacle bricks without forcing an export-cache rebuild first
    - retained runtime overlay peak-density readout now prefers backend-reported sparse stats and only falls back to a view scan when a report value is unavailable
    - sparse reporting no longer treats export-cache materialization as an implicit side effect of ordinary debug/report queries
  - the first runtime control-plane hardening slice is now live:
    - sparse cluster scheduling now reports solved cluster count, skipped cluster count, and skipped solver-cell count for oversized-region guard cases
    - the first-pass `3D` solver now applies a bounded pre-advection velocity safety clamp to prevent per-step displacement spikes from outrunning the sparse work region
    - backend reporting now exposes pre-clamp and post-clamp peak velocity/displacement metrics plus a post-project maximum absolute divergence residual
    - reporting and solver contract lanes now prove the new guard metrics route through the backend without depending on export or dense-cache side effects
  - the guard-threshold seam is now explicit instead of hardcoded-only:
    - `fluid.solver_region_cell_budget` can override the sparse solver-region cell budget
    - `fluid.max_velocity_displacement_cells` can override the pre-advection velocity-clamp limit
    - `3D` backend reporting now exposes both the effective values and whether each guard is running on defaults or config overrides
  - legacy export/parity/emitter contract lanes now read backend-owned `3D` truth views instead of writing directly through scaffold dense-state pointers
- Retained-scene save/reopen workflow exists with scene-library routing and catalog re-entry.
- The `3D` menu catalog now resolves retained scenes from the configured `Input Root` live:
  - the configured input root is treated as the source of truth for retained-scene discovery
  - direct child scene directories are scanned
  - one grouped area layer is also scanned (`<input-root>/<Area>/<Scene>/...`)
  - a directory is listed only when both `scene_authoring.json` and `scene_runtime.json` exist
  - retained-scene labels prefer authoring `scene_name` / `display_name` metadata when present, then fall back to directory names
- Catalog root selection is centralized through `physics_sim_runtime_scene_catalog_roots(...)` in `src/app/data_paths.c`:
  - configured input root
- Menu behavior now treats `Input Root` edits as live catalog changes rather than next-launch-only configuration drift.
- Stale retained-scene selection is cleared when the input root changes or the previous runtime path is no longer present in the refreshed catalog.
- The retained-scene `2D` editor viewport now partially adopts shared `core_viewport2d` math:
  - fit-to-bounds reset, cursor-anchor wheel zoom, drag-pan, and world/screen transforms route through the shared viewport contract
  - canvas rectangle choice, scene-world meaning, and broader editor gesture policy remain app-local
  - retained-scene `3D` orbit/distance/projection behavior still uses the app-local camera path
- The retained-scene `3D` editor viewport now derives orbit-distance and minimum-zoom limits from scene bounds, so large scenes can zoom farther out than the old fixed-distance ceiling allowed.
- Focused contract coverage exists for this lane:
  - `tests/scene_editor_scene_library_contract_test.c`
  - `tests/scene_editor_viewport_contract_test.c`
- Overlay writeback status:
  - `motion_mode` is runtime-consumed
  - `initial_velocity` persists through overlay/storage lanes but remains deferred until a full runtime sink is completed

## Structure
- Required scaffold lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active subsystem lanes:
  - `app`, `command`, `config`, `export`, `geo`, `import`, `input`, `physics`, `render`, `tools`, `ui`

## Verification Contract
- Build:
  - `make -C physics_sim clean && make -C physics_sim`
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
  - `make -C physics_sim test-sim-runtime-emitter-contract`
  - `make -C physics_sim test-sim-runtime-obstacle-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-emitter-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-attached-emitter-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-obstacle-contract`
  - `make -C physics_sim toolchain-contract`
- Stable validation:
  - `make -C physics_sim test-fast`
  - small deterministic non-GUI lane for common source changes; excludes
    headless integration, package, local-system/private-path, long
    visual/video, and remote-worker probes
  - includes `test-scene-menu-layout-contract`, which validates menu settings
    toggles and both legacy `Data I/O + Batch` and scene-project
    `Scene Project Cache` rectangles for normal and minimum window sizes so
    cache/headless/bottom actions do not overlap and mode-specific controls do
    not leak into the wrong panel
  - `make -C physics_sim test-stable`
  - includes 3D export contract/parity, retained-scene bridge coverage, and the shared-viewport-backed scene-editor viewport contract
- Smoke wording note:
  - `make -C physics_sim run-headless-smoke`
  - currently aliases `test-stable` rather than a separate long-lived runtime shell
- Direct headless CLI:
  - `make -C physics_sim physics_sim_headless`
  - `make -C physics_sim test-physics-sim-headless-cli`
  - `make -C physics_sim test-physics-sim-headless-scene-project-cache-output`
  - `make -C physics_sim test-physics-sim-headless-water-mode`
- Wind orientation probes:
  - `make -C physics_sim test-physics-sim-headless-wind-orientation-probe`
    is portable and uses a checked-in mesh-wedge fixture
  - `make -C physics_sim test-physics-sim-headless-dragonwind-orientation-probe`
    is a local-system probe because it defaults to a user-machine DragonWind
    runtime scene unless `DRAGONWIND_ORIENTATION_PROBE_SCENE` is supplied
- Detached runner:
  - `make -C physics_sim physics-sim-job-runner`
  - `make -C physics_sim test-physics-sim-job-runner-smoke`
  - `make -C physics_sim test-physics-sim-job-runner-policy`
- Build-only readiness:
  - `make -C physics_sim visual-harness`
- Source-run visual proof:
  - `make -C physics_sim visual-artifact`
  - writes a validated Wind projection BMP under ignored
    `visual_artifacts/source_first_frame/` and prints the final artifact path
  - uses `physics_sim_headless` with a checked-in fixture; does not require
    package launch, desktop capture, remote workers, or private scene paths
- Packaging verification:
  - `make -C physics_sim package-linux-worker-self-test`
  - `make -C physics_sim package-linux-worker-dry-run`
  - `make -C physics_sim package-linux-desktop-contract`
  - `make -C physics_sim package-linux-desktop-determinism-test`
  - `make -C physics_sim package-desktop`
  - `make -C physics_sim PACKAGE_TOOLCHAIN=fisics package-desktop`
  - `make -C physics_sim package-desktop-smoke`
  - `make -C physics_sim package-desktop-self-test`
  - `make -C physics_sim package-desktop-refresh`
- Legacy lane (known stale/failing tests can exist here by design):
  - `make -C physics_sim test-legacy`

## Release and Packaging Snapshot
- Release-readiness phases are complete through artifact flow (`RL0`-`RL3`).
- Signed/notarized/stapled distribution flow is established for production release operations.
- Package/release helper security boundary is local-operator only:
  package/release targets quote configured paths and may destructively clean
  their configured package/release/Desktop roots; app packaging stages only the
  binary, launcher, `Info.plist`, bundled dylibs, config, shader resources,
  optional icon, and optional shared fonts; Linux worker packaging stages only
  headless/job-runner binaries, config, selected public docs, and manifests;
  private/generated run lanes such as `build/agent_runs/`, `tmp/`, detached
  outputs, launcher logs, maintainer-private artifact roots, and local icon
  source copies are not package inputs unless a future target explicitly stages
  them.
- Linux worker packaging now emits truthful host-architecture metadata for
  either `linux-x86_64` or `linux-aarch64` by default:
  - `make -C physics_sim package-linux-worker`
  - `make -C physics_sim package-linux-worker-dry-run`
  - the package manifest platform follows the Linux build host architecture
  - `LINUX_WORKER_PLATFORM=<value>` remains available for explicit override
- `package-linux-worker-dry-run` is local-only validation. It validates the
  staged manifests, entrypoints, selected docs/config payload, archive contents,
  package-manifest self-test command, and each executable's required GLIBC
  symbol ceiling without upload, install, registration, raw SSH/SCP, or remote
  worker execution. The default fleet ceiling is GLIBC 2.39 and is recorded as
  `max_glibc_version` in both worker manifests.
- Worker package/cross-host handoff boundary is explicit: the local Linux
  worker archive does not install, upload, run, or register itself; worker-safe
  payloads must stage runtime scene, run config, runtime mesh documents,
  preview sidecars, and output/report roots together; Linux PC package
  upload/install/status/fetch routes through the Linux PC handoff lane, while
  VPS worker-fleet visibility, worker-exchange requests, and VPS-side runtime
  proofs route through the VPS handoff lane. Raw SSH/SCP or ad hoc remote shell
  is outside the package boundary.
- Private Linux GUI desktop packaging now has first scaffold targets:
  - `make -C physics_sim package-linux-desktop-contract`
  - `make -C physics_sim package-linux-desktop`
  - `make -C physics_sim package-linux-desktop-self-test`
  - `make -C physics_sim package-linux-desktop-determinism-test`
  - package class: `desktop_app_linux`
  - artifact role: `desktop_app`
  - runtime: `linux_gui`
  - private artifact name:
    `kinetiC-<version>-linux-x86_64-desktop-stable.tar.gz`
  - checksum sidecar:
    `kinetiC-<version>-linux-x86_64-desktop-stable.tar.gz.sha256`
  - this target is Linux-only and refuses real packaging on macOS; the local
    Mac can verify the contract shape but not the GUI package build
  - the Linux launcher runs from an unpacked archive, copies package resources
    into XDG/proof runtime roots, writes launcher logs under XDG state or an
    override, and supports display-free `--self-test`
  - real release-grade GUI proof still requires the Mac/Linux PC handoff lane:
    deterministic package target, sidecar verification, clean unpack
    self-test, real logged-in desktop launch, app-window screenshots, and
    launcher log/runtime markers
  - no `VERSION` bump, public artifact, website metadata, registry mutation,
    worker install, remote job submission, or RayTracing artifact change is
    implied by this scaffold
- The first compiler-overlay dual-toolchain contract is now active for app-local validation:
  - `clang-build` writes the default app binary to `build/clang/physics_sim`
  - `make` still copies the Clang binary to the repo-root `physics_sim` path for compatibility
  - `fisics-build` writes the overlay-enabled binary to `build/fisics/physics_sim`
  - `dump-sema` targets the retained-scene projection-domain seam under `src/import/`
  - `dump-sema-runtime-3d-solver-step` targets the first `3D` solver-step math seam under `src/app/`
  - `dump-sema-fluid2d` targets the legacy `2D` fluid solver seam under `src/physics/`
  - `dump-sema-rigid2d` targets the rigid-body solver seam under `src/physics/`
  - `dump-sema-rigid2d-collision` targets the rigid-body collision/impulse seam under `src/physics/`
  - `dump-sema-structural-runtime` targets the structural dynamic-runtime integrator seam under `src/app/structural/`
  - `dump-sema-particles2d` targets the `2D` particle integrator seam under `src/physics/particles/`
  - `dump-sema-structural-solver` targets the structural static solver seam under `src/physics/structural/`
  - `dump-sema-atmospheric-field` targets the atmospheric density/wind field generator seam under `src/app/atmospheric/`
  - `dump-sema-object-manager` targets the rigid object-manager seam under `src/physics/objects/`
  - `dump-sema-soft-body` targets the soft-body integrator seam under `src/physics/soft/`
  - `dump-sema-runtime-fields-2d` targets the `2D` backend runtime-fields/emitter seam under `src/app/`
  - `dump-sema-runtime-backend-2d` targets the adjacent `2D` backend host/runtime seam under `src/app/`
  - `dump-sema-runtime-backend-3d-emitters` targets the `3D` scaffold emitter application seam under `src/app/`
  - `dump-sema-runtime-backend-3d-runtime` targets the `3D` scaffold runtime region-step seam under `src/app/`
  - `dump-sema-runtime-backend-3d-obstacles` targets the `3D` scaffold obstacle enforcement/materialization seam under `src/app/`
  - `dump-sema-runtime-emitter` targets the emitter support seam under `src/app/`, covering resolved `position_z`, resolved `radius`, and the `3D` world-space-to-grid placement bridge
  - `dump-sema-runtime-obstacle` targets the obstacle support seam under `src/app/`, covering storage/compatibility policy, source-footprint routing, and domain-face slab bounds
  - `test-rigid2d-collision-contract` directly validates the deeper rigid-body collision manifold seam under `src/physics/rigid/`
  - `test-sim-runtime-backend-2d-runtime-fields-contract` directly validates the first bounded `2D` runtime-fields/emitter seam under `src/app/`
  - `test-sim-runtime-emitter-contract` directly validates the bounded emitter support seam under `src/app/`
  - `test-runtime-mesh-preview-bridge-contract` validates runtime-scene mesh
    preview metadata, sidecar probing, and PhysicsSim fluid-role metadata
  - `test-runtime-mesh-obstacle-proxy-contract` validates default-solid runtime
    mesh obstacle behavior, actual runtime mesh voxelization, and emitter
    exclusion from solid occupancy
  - `test-runtime-scene-bridge-contract` validates projection from runtime
    scene mesh metadata into attached runtime-mesh emitters
  - `test-sim-runtime-backend-3d-runtime-mesh-emitter-contract` validates that
    mesh-attached emitters use actual runtime mesh footprints in the 3D backend
  - `test-sim-runtime-obstacle-contract` directly validates the bounded obstacle support seam under `src/app/`
  - `test-sim-runtime-backend-3d-emitter-contract` directly validates the bounded free-emitter `3D` scaffold seam under `src/app/`
  - `test-sim-runtime-backend-3d-attached-emitter-contract` directly validates the bounded attached-emitter `3D` scaffold seam under `src/app/`
  - `test-sim-runtime-backend-3d-obstacle-contract` directly validates the bounded `3D` scaffold obstacle seam under `src/app/`
  - `package-desktop` still packages the Clang build by default
  - `PACKAGE_TOOLCHAIN=fisics` is required to package the overlay-enabled binary intentionally
- Core release utilities:
  - `make -C physics_sim release-contract`
  - `make -C physics_sim release-verify-signed ...`
  - `make -C physics_sim release-notarize ...`
  - `make -C physics_sim release-staple`
  - `make -C physics_sim release-verify-notarized`
  - `make -C physics_sim release-artifact`
  - `make -C physics_sim release-distribute ...`

## Runtime Config and Data Policy
- Tracked defaults: `config/`
- Runtime mutable state: `data/runtime/`
- Generated/temp lanes are intentionally kept out of tracked defaults policy.

## Current Boundary
- `physics_sim` producer export lane is intentionally stable.
- The current local `3D` fluid-runtime boundary remains inside runtime control-plane hardening:
  - the first solver-safety/observability slice and its config seam are landed
  - the next refinement step is to widen region/materialization and solver-health diagnostics, then decide whether any of the new control seams should graduate into authored menu/runtime UI before deeper solver-quality changes
- Current local worktree drift is in retained-scene quality/usability, not in the producer export contract:
  - input-root scene-library refresh behavior
  - paired `scene_authoring.json` + `scene_runtime.json` discovery
  - shared `core_viewport2d` adoption in retained-scene `2D` editor mode
  - large-scene `3D` viewport zoom/orbit range
- Current atmospheric initializer follow-up is user runtime validation, then a Phase 5 planning boundary for volumetric rendering/cross-app handoff.
- The next major cross-program boundary is still downstream in `ray_tracing`:
  - ingest `vf3d` / `VF3H`
  - consume truthful scene metadata
  - land first-pass density-driven volume rendering
- The current local compiler-units boundary now includes the structural
  dynamic-runtime lane and the `2D` particle integrator lane in addition to
  the rigid-body lane:
  - rigid-body lane remains active:
    - `src/physics/rigid/rigid2d.c` now has an explicit semantic-dump target
      and time-typed `dt` at the solver entry
    - `src/physics/rigid/rigid2d_collision.c` now carries the first deeper
      collision-time seam through typed `dt` and explicit inverse-step fallback
  - structural dynamic-runtime lane is now also a semantic-dump customer:
    - `src/app/structural/structural_controller_runtime.c`
    - `structural_controller_runtime_step_dynamic(...)` now accepts time-typed
      `dt`
    - the gravity-ramp time bookkeeping is now explicit physical time instead
      of plain scalar seconds
    - the translational runtime clamp path now makes displacement length and
      derived translational speed explicit before writing back to the runtime
      state arrays
    - the translational Newmark predictor/corrector and explicit-integrator
      update equations now also carry explicit physical meaning for:
      - position/displacement length
      - translational velocity
      - translational acceleration
    - the remaining translational dynamic seam inside the file is now also
      explicit:
      - mass-proportional translational damping now routes through a typed
        helper
      - explicit translational net-force-to-acceleration now routes through a
        typed helper before updating runtime acceleration/velocity/position
  - this is a cleaner next customer than the static structural stiffness core
    because the dynamic runtime already exposes honest time, displacement, and
    speed boundaries without first needing derived-unit cleanup for area,
    inertia, or stiffness storage
  - `2D` particle integrator lane is now also a semantic-dump customer:
    - `src/physics/particles/particles2d.c`
    - `particles2d_step(...)` now accepts time-typed `dt`
    - particle lifetime decrement is now explicit physical time
    - gravity application is now explicit translational acceleration into
      translational velocity
    - position integration is now explicit translational velocity into length
  - the particle/fluid compatibility seam is now isolated into a named bridge:
    - fluid-grid velocity sampling intentionally remains scalar
    - the particle lane now reuses the shared `fluid2d_sample_velocity(...)`
      API instead of owning a second inline bilinear sampler
    - particle positions are still sampled directly in legacy grid coordinates
    - the compatibility blend now lives in one helper instead of being mixed
      inline with the rest of the particle integrator
  - the structural static solver lane is now also a semantic-dump customer:
    - `src/physics/structural/structural_solver.c`
    - member-axis geometry now routes through one typed helper
    - nodal planar load application now routes through a typed force helper
    - cross-sectional area and second moment of area now route through typed
      section-property helpers, and axial stress now resolves through an
      explicit force-over-area pressure helper
    - repeated material/stiffness formulas now also route through localized
      typed helper seams for Young's modulus, axial stiffness, and bending
      stiffness
    - the remaining limit is still runtime storage and assembly vocabulary:
      the solver does not yet carry full modulus/stiffness families through
      scene storage or matrix assembly outputs
  - the atmospheric field generator is now also a semantic-dump customer:
    - `src/app/atmospheric/atmospheric_field.c`
    - atmospheric sample velocity fields now carry explicit velocity semantics
    - localized base-wind and turbulence-strength locals now route the `2D`
      and `3D` curl-noise wind expressions through one typed helper seam
    - the remaining limit is still vocabulary coverage:
      density remains scalar, and normalized sample/noise coordinates remain
      intentionally untyped until a real density-family contract exists
  - the rigid object-manager is now also a semantic-dump customer:
    - `src/physics/objects/object_manager.c`
    - `object_manager_step(...)` now carries explicit time semantics at the
      rigid-world handoff into `rigid2d_step(...)`
    - circle radius, box half-extents, and object position now route through a
      localized length helper seam before writing into body storage
    - the remaining limit is still polygon/local-vertex cleanup:
      `add_poly(...)` still accepts raw local vertex arrays without a typed
      per-vertex length contract
  - the deeper rigid collision manifold lane is now also directly validated:
    - `src/physics/rigid/rigid2d_collision.c`
    - typed `dt` and explicit inverse-step fallback remain in place
    - restitution thresholding now routes through a typed
      relative-normal-velocity helper
    - penetration bias velocity now routes through a typed
      penetration-length / inverse-time helper
    - tangent-speed extraction and the first friction threshold checks now
      route through typed velocity helpers instead of raw scalar compares
    - `make -C physics_sim test-rigid2d-collision-contract` now directly
      exercises the bounded manifold lane with isolated restitution and
      friction checks
  - the soft-body reference solver lane is now also a semantic-dump customer:
    - `src/physics/soft/soft_body.c`
    - `soft_body2d_step(...)` now carries explicit time semantics
    - the file now owns a bounded real solver reference:
      node storage, spring storage, pinned-node semantics via `mass <= 0`,
      gravity force accumulation, spring pull/damping, explicit
      velocity -> displacement -> position integration, and iterative
      spring-constraint relaxation with velocity rebuild from corrected
      positions, plus triangle area constraints for basic shape preservation
    - `make -C physics_sim test-soft-body-contract` now exercises the lane
      directly
    - the remaining limit is still solver depth:
      this is a bounded spring/area reference, not a full collision-aware or
      volume-preserving production soft-body system
  - the `2D` backend runtime-fields lane is now also a semantic-dump customer:
    - `src/app/sim_runtime_backend_2d_runtime_fields.c`
    - `backend_2d_apply_emitters(...)` now treats `dt` as explicit physical
      time with a typed zero-time no-op guard
    - the first honest emitter-physics slice is now explicit:
      - scalar total-strength accumulation still stays untyped because the
        file mixes density-source and velocity-source magnitudes
      - emitted velocity deltas for density-source, jet, and sink paths now
        route through typed velocity helpers before writing into `fluid2d`
      - the free-emitter radial injection path, shared-mask path, and
        attached-object/import footprint application path now all route
        through named helpers instead of inline mask-building branches
      - the moving-obstacle mask + obstacle-velocity writeback path now also
        routes through localized helpers instead of an inline import/circle/poly
        splat branch
    - `make -C physics_sim test-sim-runtime-backend-2d-runtime-fields-contract`
      now exercises the bounded lane directly:
      - zero-`dt` emitter application is a no-op
      - free velocity-jet emission injects positive density and x-velocity
        into the `2D` field
      - attached-object velocity-jet emission injects density and x-velocity
        through the extracted footprint helper lane
      - a moving circle obstacle writes both obstacle mask coverage and
        obstacle x-velocity through the extracted runtime-field bridge
    - the remaining limit is no longer the obstacle bridge:
      scalar grid-cell geometry and mixed density/velocity strength semantics
      remain compatibility carriers rather than honest world-unit lanes

- The next compiler-units boundary after the localized `2D` runtime-fields
  bridge slice is now also landed in the adjacent backend host file:
  - `src/app/sim_runtime_backend_2d.c`
  - it is now the thirteenth semantic-dump customer in the rollout
  - the first bounded host/runtime seam now covers:
    - `backend_2d_apply_boundary_flows(...)` with typed `dt` and typed zero-time no-op handling
    - `backend_2d_step(...)` with the same typed zero-time guard at the host-step boundary
    - `backend_2d_inject_object_motion(...)` with typed object-body velocity locals and a localized velocity injection helper into `fluid2d`
    - `backend_2d_capture_atmospheric_seed_stats(...)` with typed wind-velocity locals and typed speed magnitude recovery
  - `make -C physics_sim test-sim-runtime-backend-2d-contract` now directly exercises the bounded host seam:
    - zero-`dt` boundary-flow application is a no-op
    - positive-`dt` boundary emission injects density and velocity into the field
    - object-motion velocity injection writes velocity into the `2D` fluid field
  - the remaining limit in this file is now host/control compatibility geometry rather than the first physical runtime seam
  - the adjacent `3D` scaffold lane now also has its first explicit sema coverage:
    - `src/app/sim_runtime_backend_3d_emitters.c`
      - emitter total-strength over `dt`, footprint world-volume / voxel-volume normalization, per-cell density and velocity deltas, and thermal buoyancy routing are now explicit semantic-dump customers
      - direct proof already existed and remains green:
        - `make -C physics_sim test-sim-runtime-backend-3d-emitter-contract`
        - `make -C physics_sim test-sim-runtime-backend-3d-attached-emitter-contract`
    - `src/app/sim_runtime_backend_3d_runtime.c`
      - the region-limited runtime step with typed `dt`, solver-region budgeting, and displacement-clamp metric recovery is now also an explicit semantic-dump customer
    - `src/app/sim_runtime_backend_3d_obstacles.c`
      - obstacle-volume rebuild, compatibility-slice sync, and live obstacle enforcement zeroing are now also explicit semantic-dump customers
      - direct proof already existed and remains green:
        - `make -C physics_sim test-sim-runtime-backend-3d-obstacle-contract`
    - together these three files move the remaining meaningful uncovered `3D` physics surface down into narrower support lanes rather than core solver/runtime math

## History and Deep Lane References
- Detailed execution slices, archived plans, and deep phase logs are kept in
  maintainer-private docs outside the public repository.
- Use this public file as the compressed current-state contract.
