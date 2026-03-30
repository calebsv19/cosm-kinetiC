# Physics Sim Current Truth

Last updated: 2026-03-29

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
- runtime UI zoom contract (font-pass `PS-F1` + `PS-F2`):
  - `ui.text_zoom_step` now loads/saves through the same runtime app-state lane
  - clamp policy: `[-4, +5]` steps
  - percent mapping: `100 + step*10` with hard clamp `[60, 180]`
  - `Cmd/Ctrl +/-/0` shortcut capture is routed through `input_poll_events()` and consumed before context callbacks
  - zoom updates are persisted immediately to `data/runtime/app_state.json` in active menu/editor/structural loops
  - live font reload is wired for menu/editor/structural runtime lanes
  - `PS-F3` layout safety updates are now active:
    - menu row/list/control geometry and text alignment derive from current font metrics
    - scene editor panel control/list geometry reflows on zoom and window-size changes
    - structural runtime and structural preset editor HUD/tooltips now use font-driven line spacing
    - menu font reload now swaps atomically so failed reload attempts do not blank active UI text

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
  - scaffold migration complete; next structured lane is post-scaffold font-size standardization plan:
  - `../docs/private_program_docs/physics_sim/2026-03-29_physics_sim_post_scaffold_font_size_pass_plan.md`
  - current status in that lane:
    - `PS-F0` complete (font/text/input mapping + risk capture)
    - `PS-F1` complete (runtime zoom contract + menu post-tier scaling)
    - `PS-F2` complete (keyboard shortcuts + live refresh wiring)
    - `PS-F3` complete (layout safety pass)
    - `PS-F4` complete (pane-by-pane visual audit + overlap/clip hardening, including width-fit text safety in menu controls and editor widgets/panels)
    - `PS-F5` complete (verification/docs/memory/commit wrap-up executed with title `Post-Scaffold Font Size Standardization`)
  - `test-stable` remains the baseline non-interactive regression gate during the font pass
