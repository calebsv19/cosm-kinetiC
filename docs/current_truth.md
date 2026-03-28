# Physics Sim Current Truth

Last updated: 2026-03-28

## Program Identity
- Repository directory: `physics_sim/`
- Public product name in README: `Physics Sim`
- Primary runtime entry path today:
  - `src/main.c` (`main()` delegates to `physics_sim_app_main(...)`)
  - canonical lifecycle wrapper entry:
    - `include/physics_sim/physics_sim_app_main.h`
    - `src/app/physics_sim_app_main.c`

## Current Structure
- Required scaffold lanes are present:
  - `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active source subsystem lanes:
  - `app`, `command`, `config`, `export`, `geo`, `import`, `input`, `physics`, `render`, `tools`, `ui`
- Header strategy:
  - include-dominant (`77` headers in `include/`, `6` private headers in `src/`)
  - include layout is currently domain-first (`include/app`, `include/physics`, etc.)

## Runtime/Verification Contract (Current)
- Build:
  - `make -C physics_sim clean && make -C physics_sim`
- Scaffold smoke gate (non-interactive):
  - `make -C physics_sim run-headless-smoke`
- Visual harness build gate:
  - `make -C physics_sim visual-harness`

Stable test lane:
- `make -C physics_sim test-stable`
- current composition:
  - `test-manifest-to-trace-export`
  - `test-vf2d-pack-dataset-parity`
  - `test-kitviz-field-adapter`

Legacy test lane:
- `make -C physics_sim test-legacy`
- current composition (known failing at `PS-S0` baseline):
  - `test-shared-theme-font-adapter`
    - failure message:
      - `shared_theme_font_adapter_test: theme should be disabled by default`

## Shared Dependency Snapshot
- Shared libs actively consumed by current `makefile`:
  - `core_base`, `core_io`, `core_data`, `core_pack`, `core_scene`, `core_theme`, `core_font`
  - `kit_viz`
  - `vk_renderer`
  - `timer_hud`

## Runtime Config Persistence
- committed defaults:
  - `config/app.json`
  - `config/custom_preset.txt` (fallback preset seed)
- runtime-persisted mutable state:
  - `data/runtime/app_state.json`
  - `data/runtime/custom_preset.txt`
- load order:
  - runtime file first, default fallback when runtime state is missing
- save path:
  - runtime lane only (`data/runtime/*`) so normal app runs do not mutate tracked defaults

## Temp/Generated Lane Snapshot
- currently gitignored:
  - `build/`
  - `export/`
  - `ide_files/`
  - `timerhud/`
  - `tmp/`
  - `data/runtime/`
  - `data/snapshots/`
  - tool binaries and `.dSYM` lanes
- runtime/state lane policy is now locked for scaffold migration:
  - tracked defaults stay in `config/`
  - mutable runtime state writes to ignored `data/runtime/`

## Active Scaffold Migration State
- Private migration plan:
  - `../docs/private_program_docs/physics_sim/2026-03-28_physics_sim_scaffold_standardization_switchover_plan.md`
- Baseline freeze:
  - `../docs/private_program_docs/physics_sim/2026-03-28_ps_s0_baseline_freeze_and_mapping.md`
- Completed phases:
  - `PS-S0`, `PS-S1`, `PS-S2`, `PS-S3`, `PS-S4`, `PS-S5`
- Next phase:
  - scaffold migration complete; next work moves to normal feature/fix flow with `test-stable` as baseline gate
