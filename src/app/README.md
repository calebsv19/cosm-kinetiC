# `src/app/`

Scene orchestration lives here. These files wrap the physics primitives into something usable by the main loop.

- `app_config.c` – builds the default `AppConfig` struct (window size, grid size, timestep clamps, and solver tuning constants). Other systems treat this as read-only runtime configuration.
- `scene_menu.c` – lightweight SDL menu focused on the four editable custom slots. Each slot can be renamed, edited, and saved, and grid settings can still be tweaked before launching the sim. It talks to `scene_editor.c` and `preset_io.c` so edits persist across runs.
- `preset_io.c` – serializer that saves/loads the custom slot library (`config/custom_preset.txt`) so your edits are restored the next time the app launches.
- `scene_presets.c` – catalog of built-in emitter layouts (hotspots, jets, etc.) consumed by both the menu and runtime.
- `scene_state.c` – owns the lifetime of the `SceneState`. It creates/destroys the `Fluid2D` grid, applies brush samples, drives preset emitters, and responds to queued commands (pause toggles, clears).
- `scene_controller.c` – extracted main loop that initializes SDL/rendering, pumps input, drains the command bus, drains the stroke buffer, steps physics, handles snapshot exports, and draws each frame.
- `editor/scene_editor.c` – the high-level scene editor logic: handles slot renaming, input routing, and ties the canvas/panel helpers together.
- `editor/scene_editor_canvas.c` – canvas helper that draws emitters, tooltips, and the preset title, plus the hit-testing/projection math.
- `editor/scene_editor_widgets.c` – UI primitives (buttons, numeric fields) shared across the editor panel.

`scene_state.c` is the place to hook in new solvers: inject additional data into `SceneState`, update it inside `scene_apply_input`, respond to commands, and step it next to the fluid loop that already reads `AppConfig`.
