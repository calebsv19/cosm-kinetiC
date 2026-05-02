# Physics Sim Current Truth

Last updated: 2026-04-29

## Program Identity
- Repository directory: `physics_sim/`
- Public product name: `Physics Sim`
- Primary runtime entry:
  - `src/main.c` (`main()` -> `physics_sim_app_main(...)`)
  - wrapper shell: `include/physics_sim/physics_sim_app_main.h`, `src/app/physics_sim_app_main.c`

## Current Shipped State
- Producer-side truthful `3D` export is complete (through `PSBU-11D`).
- Authoritative volumetric (`XYZ`) runs now emit:
  - raw `.vf3d`
  - additive `VF3H` `.pack`
  - truthful `manifest.json` / `scene_bundle.json` metadata (`frame_contract=vf3d`, `space_mode=3d`, `axis_authority=xyz`)
- First-pass parity fixture is locked and deterministic (tiny-domain proof lane).
- Downstream consumer work remains separate:
  - `ray_tracing` ingest/render handoff is the next cross-program boundary.

## Runtime and Editor Snapshot
- Runtime/editor retained-scene lanes are active and structurally separated from legacy compatibility mapping.
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
  - includes 3D export contract/parity and retained-scene bridge coverage
- Smoke and harness:
  - `make -C physics_sim run-headless-smoke`
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
- Current local worktree drift is in retained-scene quality/usability, not in the producer export contract:
  - input-root scene-library refresh behavior
  - paired `scene_authoring.json` + `scene_runtime.json` discovery
  - large-scene `3D` viewport zoom/orbit range
- The next major cross-program boundary is still downstream in `ray_tracing`:
  - ingest `vf3d` / `VF3H`
  - consume truthful scene metadata
  - land first-pass density-driven volume rendering

## History and Deep Lane References
- Detailed execution slices, archived plans, and deep phase logs are kept in private docs:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/physics_sim/`
- Use this public file as the compressed current-state contract.
