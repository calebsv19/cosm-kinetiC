# Physics Sim Docs Index

Start here for public repository documentation.

## Scaffold State
- `docs/current_truth.md`: current runtime structure, truthful `3D` export state, and verification snapshot.
- `docs/future_intent.md`: next public direction, including the deferred downstream `ray_tracing` `3D` ingest/render handoff.
- migration-friendly verification gates:
  - `make -C physics_sim run-headless-smoke`
  - `make -C physics_sim visual-harness`
  - `make -C physics_sim test-stable`
  - `make -C physics_sim test-legacy`

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview, execution flow, and controls.
- `docs/KEYBINDS.md`: full keybind list across fluid and structural lanes.
- `docs/desktop_packaging.md`: desktop app bundle workflow, launcher contract, and verification commands.

## Current Published State
- `physics_sim` is now a truthful `3D` producer on the export side:
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

## Runtime Persistence Policy
- tracked defaults remain under `config/`
- mutable runtime state persists under ignored `data/runtime/`

## Private Planning Docs
- Private scaffold plans/checklists are kept in the workspace private docs bucket:
  - `../../docs/private_program_docs/physics_sim/`
