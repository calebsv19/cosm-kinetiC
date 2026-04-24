# `src/app/`

Scene orchestration lives here. These files wrap the physics primitives into something usable by the main loop.

- `app_config.c` – builds the default `AppConfig` struct (window size, grid size, timestep clamps, solver tuning constants, quality profile index, render blur flag, and headless defaults). Other systems treat this as read-only runtime configuration.
- `quality_profiles.c` – defines the built-in Preview/Balanced/High/Deep presets so menu selections can snap to known solver/grid configurations.
- `scene_menu.c` – lightweight SDL menu focused on the editable custom slots. Each slot can be renamed, edited, and saved, and the right-hand panel now exposes grid controls, quality cycling, and headless batch controls (toggle + inline frame editor). It talks to `scene_editor.c` and `preset_io.c` so edits persist across runs.
- `preset_io.c` – serializer that saves/loads the custom slot library (`config/custom_preset.txt`) so your edits are restored the next time the app launches.
- `scene_presets.c` – catalog of built-in emitter layouts (hotspots, jets, etc.) consumed by both the menu and runtime.
- `scene_state.c` – owns the lifetime of the `SceneState`. It creates/destroys the `Fluid2D` grid, applies brush samples, drives preset emitters, builds/caches the solid obstacle mask plus its distance transform, and responds to queued commands (pause toggles, clears). The cached mask/distance data is fed to the solver and renderer overlays so we only rasterize obstacles when they actually change.
- `scene_controller.c` – extracted main loop that initializes SDL/rendering, pumps input, drains the command bus, drains the stroke buffer, steps physics, handles snapshot exports, and draws each frame. IR1 seams are explicit with typed input frame contracts (`scene_controller_input_intake_phase`, `scene_controller_input_normalize_phase`, `scene_controller_input_route_phase`, `scene_controller_input_invalidate_phase`) and optional `PHYSICS_SIM_IR1_DIAG=1` logs. RS1 seams remain explicit with typed frame contracts: `scene_controller_update_phase(...)`, `scene_controller_render_derive_phase(...)`, and `scene_controller_render_submit_phase(...)`. Headless batches run through the same path but skip presentation/input while still showing TimeScope timers.
- `scene_loop_policy.c` – mode-aware wait timeout policy shared by menu/runtime loops. Keeps full-ramp behavior while simulation/headless work is active and uses bounded idle waits in menu/editor-like states.
- `scene_loop_diag.c` – emits schema-locked (`schema=1`) `LoopDiag` snapshots for blocked-vs-active calibration using `PHYSICS_SIM_LOOP_DIAG_*` env flags.
- `editor/scene_editor.c` – the high-level scene editor logic: handles slot renaming, input routing, and ties the canvas/panel helpers together.
- `editor/scene_editor_session.c` – app-local editor-session seam above retained runtime-scene intake. Keeps canonical retained-scene bootstrap plus mirrored legacy editor-selection state separate from the legacy preset compatibility lane.
- `editor/scene_editor_pane_host.c` – app-local pane-host wrapper that maps editor shell roles onto shared `core_pane` split geometry.
- `editor/scene_editor_canvas.c` – render helper for the editor canvas (objects/imports/emitters, tooltips, preset title, boundary flows).
- `editor/scene_editor_canvas_geom.c` – math helpers (projection, normalized ↔ pixel conversions, handle sizing/placement).
- `editor/scene_editor_canvas_hit.c` – hit testing for emitters/objects/imports and boundary edges plus emitter-handle hit tests.
- `editor/scene_editor_widgets.c` – UI primitives (buttons, numeric fields) shared across the editor panel.

`scene_state.c` is the place to hook in new solvers: inject additional data into `SceneState`, update it inside `scene_apply_input`, respond to commands, and step it next to the fluid loop that already reads `AppConfig`.
