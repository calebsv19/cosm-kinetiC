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

## Runtime Persistence Policy
- tracked defaults remain under `config/`
- mutable runtime state persists under ignored `data/runtime/`

## Private Planning Docs
- Private scaffold plans/checklists are kept in the workspace private docs bucket:
  - `../../docs/private_program_docs/physics_sim/`
