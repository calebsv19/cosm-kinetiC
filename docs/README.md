# Physics Sim Docs Index

Start here for public repository documentation.

## Scaffold State
- `docs/current_truth.md`: current scaffold/runtime structure and verification snapshot.
- `docs/future_intent.md`: scaffold convergence intent and next migration phases.
- migration-friendly verification gates:
  - `make -C physics_sim run-headless-smoke`
  - `make -C physics_sim visual-harness`
  - `make -C physics_sim test-stable`
  - `make -C physics_sim test-legacy`

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview, execution flow, and controls.
- `docs/KEYBINDS.md`: full keybind list across fluid and structural lanes.
- `docs/desktop_packaging.md`: desktop app bundle workflow, launcher contract, and verification commands.

## Runtime Persistence Policy
- tracked defaults remain under `config/`
- mutable runtime state persists under ignored `data/runtime/`

## Private Planning Docs
- Private scaffold plans/checklists are kept in the workspace private docs bucket:
  - `../../docs/private_program_docs/physics_sim/`
