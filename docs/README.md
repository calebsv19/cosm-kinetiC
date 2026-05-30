# kinetiC Docs Index

Start here for public repository documentation.

Public identity:
- packaged desktop product: `kinetiC`
- repository/program key: `physics_sim`

## Scaffold State
- `docs/current_truth.md`: current runtime structure, truthful `3D` export state, and verification snapshot.
- `docs/future_intent.md`: next public direction, including the deferred downstream `ray_tracing` `3D` ingest/render handoff.
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
  - `make -C physics_sim test-soft-body-contract`
  - `make -C physics_sim toolchain-contract`
  - `make -C physics_sim test-stable`
  - `make -C physics_sim run-headless-smoke`
    - currently aliases `test-stable` rather than a separate long-lived runtime lane
  - `make -C physics_sim visual-harness`
    - build-only readiness gate, not an unattended execution surface
  - `make -C physics_sim test-legacy`

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview, execution flow, and controls.
- `docs/KEYBINDS.md`: full keybind list across fluid and structural lanes.
- `docs/headless_cli.md`: direct `physics_sim_headless` command for
  retained runtime-scene volume runs, detached runner usage, progress
  reporting, and agent-owned output-directory policy.
- `docs/desktop_packaging.md`: desktop app bundle workflow, launcher contract, and verification commands.

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
- the retained-scene menu/editor lane is also current:
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
  - runs write `run_summary.json` and `run_progress.json`
  - `run_progress.json` now exposes solver-step progress inside a frame
  - non-empty output roots are rejected unless `--overwrite` is provided
  - skip-present volume-only runs avoid SDL video/renderer initialization
  - `--save-render-frames` and `--present` still require the existing renderer path
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
  - `make -C physics_sim toolchain-contract`
  - `make -C physics_sim package-linux-worker-self-test`
  - `make -C physics_sim package-desktop`
  - `make -C physics_sim PACKAGE_TOOLCHAIN=fisics package-desktop`
  - desktop packaging stays Clang-default unless `PACKAGE_TOOLCHAIN=fisics` is set
  - Linux worker packaging now follows the Linux build-host architecture by
    default (`linux-x86_64` or `linux-aarch64`)

## Runtime Persistence Policy
- tracked defaults remain under `config/`
- mutable runtime state persists under ignored `data/runtime/`

## Private Planning Docs
- Private scaffold plans/checklists are kept in the workspace private docs bucket:
  - `../../docs/private_program_docs/physics_sim/`
