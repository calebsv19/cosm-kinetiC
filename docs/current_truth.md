# kinetiC Current Truth

Last updated: 2026-06-13

## Program Identity
- Repository directory: `physics_sim/`
- Public product name: `kinetiC`
- Internal/repo/runtime identifiers still use `physics_sim` and `Physics Sim`
  in launcher, log, binary, and source-level contracts where required
- Primary runtime entry:
  - `src/main.c` (`main()` -> `physics_sim_app_main(...)`)
  - wrapper shell: `include/physics_sim/physics_sim_app_main.h`, `src/app/physics_sim_app_main.c`

## Current Shipped State
- Producer-side truthful `3D` export is complete (through `PSBU-11D`).
- Direct retained-scene headless CLI volume runs are available through
  `physics_sim_headless`.
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
- Detached status schema is `physics_sim_detached_job_status_v1` and exposes
  `queued`, `starting`, `running`, `stalled`, `completed`, `failed`, and
  `cancelled` states without requiring a live PTY.
- Authoritative volumetric (`XYZ`) runs now emit:
  - raw `.vf3d`
  - additive `VF3H` `.pack`
  - truthful `manifest.json` / `scene_bundle.json` metadata (`frame_contract=vf3d`, `space_mode=3d`, `axis_authority=xyz`)
- First-pass parity fixture is locked and deterministic (tiny-domain proof lane).
- Downstream consumer work remains separate:
  - `ray_tracing` ingest/render handoff is the next cross-program boundary.
- Atmospheric preset initialization is now available as an app-local current-state lane:
  - standalone Atmospheric `2D` and `3D` modes seed deterministic density and velocity fields from the atmospheric sampler instead of starting blank
  - normal `3D` fluid/box presets can opt into the same procedural initializer through the compact `Atmo Init` settings control while keeping their normal mode identity
  - custom preset persistence uses v14 for the optional `3D` atmospheric initial-state bit alongside embedded `ATMOS` settings; older v13 files load with that optional layer off
  - exported Atmospheric `3D` `.vf3d` / `VF3H` `.pack` frames can be selected as session-local warm starts for Atmospheric `3D`; warm-start file paths are runtime config, not portable preset-owned data
  - runtime reports and the HUD distinguish blank starts, standalone atmospheric procedural starts, optional atmospheric procedural starts, and loaded warm-start starts

## Runtime and Editor Snapshot
- Runtime/editor retained-scene lanes are active and structurally separated from legacy compatibility mapping.
- Menu/editor shell buttons now use bounded shared `kit_ui` button
  spec/state/style semantics through app-local `physics_sim_ui_button.*`
  wrappers, while SDL drawing, palette tuning, and button placement remain
  app-local.
- Agent/headless retained-scene runs can now bypass menu interaction:
  - build with `make -C physics_sim physics_sim_headless`
  - run `physics_sim/physics_sim_headless --runtime-scene <scene_runtime.json> --frames <n> --output-root <dir> --progress-interval <n> --save-volume-frames`
  - output includes `run_summary.json` under the selected output root
  - `run_progress.json` now advances inside a frame instead of only at frame boundaries
  - volume exports keep the existing `volume_frames/<Preset>/` layout with
    `.vf3d`, `.pack`, `manifest.json`, and `scene_bundle.json`
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
  - `make -C physics_sim test-stable`
  - includes 3D export contract/parity, retained-scene bridge coverage, and the shared-viewport-backed scene-editor viewport contract
- Smoke wording note:
  - `make -C physics_sim run-headless-smoke`
  - currently aliases `test-stable` rather than a separate long-lived runtime shell
- Direct headless CLI:
  - `make -C physics_sim physics_sim_headless`
  - `make -C physics_sim test-physics-sim-headless-cli`
- Detached runner:
  - `make -C physics_sim physics-sim-job-runner`
  - `make -C physics_sim test-physics-sim-job-runner-smoke`
  - `make -C physics_sim test-physics-sim-job-runner-policy`
- Build-only readiness:
  - `make -C physics_sim visual-harness`
- Packaging verification:
  - `make -C physics_sim package-linux-worker-self-test`
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
- Linux worker packaging now emits truthful host-architecture metadata for
  either `linux-x86_64` or `linux-aarch64` by default:
  - `make -C physics_sim package-linux-worker`
  - the package manifest platform follows the Linux build host architecture
  - `LINUX_WORKER_PLATFORM=<value>` remains available for explicit override
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
- Detailed execution slices, archived plans, and deep phase logs are kept in private docs:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/physics_sim/`
- Use this public file as the compressed current-state contract.
