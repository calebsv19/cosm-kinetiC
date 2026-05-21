# kinetiC Current Truth

Last updated: 2026-05-20

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
- Agent/headless retained-scene runs can now bypass menu interaction:
  - build with `make -C physics_sim physics_sim_headless`
  - run `physics_sim/physics_sim_headless --runtime-scene <scene_runtime.json> --frames <n> --output-root <dir> --progress-interval <n> --save-volume-frames`
  - output includes `run_summary.json` under the selected output root
  - `run_progress.json` now advances inside a frame instead of only at frame boundaries
  - volume exports keep the existing `volume_frames/<Preset>/` layout with
    `.vf3d`, `.pack`, `manifest.json`, and `scene_bundle.json`
  - skip-present volume-only runs avoid SDL video and renderer initialization
  - render-frame capture and presented runs still use the existing window/Vulkan renderer path
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
  - `make -C physics_sim package-desktop`
  - `make -C physics_sim package-desktop-smoke`
  - `make -C physics_sim package-desktop-self-test`
  - `make -C physics_sim package-desktop-refresh`
- Legacy lane (known stale/failing tests can exist here by design):
  - `make -C physics_sim test-legacy`

## Release and Packaging Snapshot
- Release-readiness phases are complete through artifact flow (`RL0`-`RL3`).
- Signed/notarized/stapled distribution flow is established for production release operations.
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

## History and Deep Lane References
- Detailed execution slices, archived plans, and deep phase logs are kept in private docs:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/physics_sim/`
- Use this public file as the compressed current-state contract.
