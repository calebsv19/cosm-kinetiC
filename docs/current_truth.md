# Physics Sim Current Truth

Last updated: 2026-03-31

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
  - include-dominant (`79` headers in `include/`, `6` private headers in `src/`)
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
  - `test-sim-mode-route-contract`
  - `test-preset-io-dimensional-contract`

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
- space mode runtime contract (`PS-U1`) is active:
  - canonical enum/state:
    - `SPACE_MODE_2D`
    - `SPACE_MODE_3D`
    - `AppConfig.space_mode`
  - persisted keys:
    - tracked default `config/app.json` includes `"simulation.space_mode": 0`
    - runtime `data/runtime/app_state.json` includes mutable `simulation.space_mode`
    - loader fallback key supported: `simulation.spaceMode`
  - menu-level selector:
    - `Space: 2D`
    - `Space: 3D (Scaffold)`
- mode adapter seam (`PS-U2`) is active:
  - centralized world/view routing for editor canvas projection and hit mapping:
    - `include/app/space_mode_adapter.h`
    - `src/app/space_mode_adapter.c`
    - `src/app/editor/scene_editor_canvas_geom.c`
  - runtime solver/backend route contract now resolves through:
    - `SimModeRoute` in `include/app/sim_mode.h`
    - `sim_mode_resolve_route(...)` in `src/app/sim_modes/sim_mode_dispatch.c`
    - scene-controller runtime entrypoint route selection in `src/app/scene_controller.c`
  - current 3D behavior remains controlled/scaffolded and intentionally falls back to canonical 2D projection/backend behavior
- additive scene/object dimensional contract (`PS-U3`) is active:
  - additive dimensional fields are present in preset contracts with backward-safe defaults:
    - `FluidEmitter.position_z`
    - `FluidEmitter.dir_z`
    - `PresetObject.position_z`
    - `PresetObject.size_z`
    - `ImportedShape.position_z`
    - `FluidScenePreset.dimension_mode`
  - preset serialization upgraded to additive v12 contract in `src/app/preset_io.c`:
    - loader preserves v11 compatibility and defaults omitted dimensional fields to 2D-safe values
    - saver emits dimensional metadata/fields in v12 format
  - regression coverage includes:
    - legacy omitted-field fallback (`v11` file)
    - additive field roundtrip (`v12` file)
    - test target: `test-preset-io-dimensional-contract`
- backend separation + mode routing contract (`PS-U4`) is active:
  - runtime route ownership is explicit and propagated end-to-end:
    - `SceneState.mode_route` now stores resolved backend/projection route
    - scene creation path accepts a resolved route:
      - `scene_create(..., const SimModeRoute *mode_route)`
  - backend/projection routing remains centralized under adapter/dispatch seams:
    - `sim_mode_resolve_route(...)` resolves canonical backend lane contract
    - editor canvas now consumes resolved route directly:
      - `scene_editor_canvas_set_mode_route(...)`
      - route-driven view-context assembly in `scene_editor_canvas_geom.c`
    - `space_mode_adapter` now supports explicit projection-mode contexts (`*_ex`, `*_for_route`)
  - render/HUD now consumes backend-selected route state (not raw config mode):
    - `RendererHudInfo` includes requested/projection space modes and backend-lane fields
    - HUD displays:
      - effective `Space` route (`requested -> projection` when fallback applies)
      - backend lane (`Canonical 2D` vs `Controlled 3D lane`)
  - route regression coverage added:
    - test target: `test-sim-mode-route-contract`
    - verifies canonical 2D route, controlled 3D lane fallback route, and invalid-mode clamp behavior
- UX + editor parity layer (`PS-U5`) is active:
  - menu lane now shows explicit controlled-3D scaffold guidance when `Space: 3D` is selected:
    - `3D lane scaffold: canonical 2D backend route`
    - source: `src/app/scene_menu.c`
  - scene editor now shows explicit space-mode state + scaffold policy in canvas-side guidance lines:
    - `Space Mode: 2D/3D (Scaffold)`
    - `3D scaffold uses canonical 2D solver/projection route.`
    - source: `src/app/editor/scene_editor_panel.c`
  - runtime HUD route visibility from `PS-U4` remains the canonical run-lane route indicator:
    - `Space: <requested> [-> <projection>]`
    - `Backend: Canonical 2D lane` / `Controlled 3D lane`

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
- Active post-scaffold lanes:
  - completed font-size standardization lane:
    - `../docs/private_program_docs/physics_sim/2026-03-29_physics_sim_post_scaffold_font_size_pass_plan.md`
    - `PS-F0` through `PS-F5` complete
  - active trio 2D/3D parity lane:
    - `../docs/private_program_docs/physics_sim/2026-03-30_physics_sim_2d_3d_parity_with_line_drawing_plan.md`
    - `PS-U0` complete (baseline freeze + gap map + tracker sync)
    - `PS-U1` complete (space mode runtime contract + persistence + menu selector)
    - `PS-U2` complete (mode adapter seam for world/view/solver routing)
    - `PS-U3` complete (additive scene/object dimensional contract + compatibility tests)
    - `PS-U4` complete (backend separation + route propagation + route contract tests)
    - `PS-U5` complete (UX + editor parity mode-visibility + scaffold guidance)
    - `PS-U6` complete (verification + docs + memory closeout)
    - execution logs:
      - `../docs/private_program_docs/physics_sim/2026-03-30_ps_u0_baseline_freeze_and_gap_map.md`
      - `../docs/private_program_docs/physics_sim/2026-03-31_ps_u1_space_mode_runtime_contract.md`
      - `../docs/private_program_docs/physics_sim/2026-03-31_ps_u2_mode_adapter_seam.md`
      - `../docs/private_program_docs/physics_sim/2026-03-31_ps_u3_additive_scene_object_contract.md`
      - `../docs/private_program_docs/physics_sim/2026-03-31_ps_u4_backend_separation_and_mode_routing.md`
      - `../docs/private_program_docs/physics_sim/2026-03-31_ps_u5_ux_editor_parity_layer.md`
      - `../docs/private_program_docs/physics_sim/2026-03-31_ps_u6_verification_docs_memory_closeout.md`
    - parity lane status: complete (`PS-U0` through `PS-U6`)
  - `test-stable` remains the baseline non-interactive regression gate during parity rollout slices
