# `src/app/`

Scene orchestration lives here. These files wrap the physics primitives into something usable by the main loop.

- `app_config.c` – builds the default `AppConfig` struct (window size, grid size, timestep clamps, and solver tuning constants). Other systems treat this as read-only runtime configuration.
- `scene_menu.c` – lightweight SDL menu that lets you pick a preset scene and grid resolution before the simulation starts.
- `scene_presets.c` – catalog of built-in emitter layouts (hotspots, jets, etc.) consumed by both the menu and runtime.
- `scene_state.c` – owns the lifetime of the `SceneState`. It creates/destroys the `Fluid2D` grid, applies brush samples, drives preset emitters, and responds to queued commands (pause toggles, clears).
- `scene_controller.c` – extracted main loop that initializes SDL/rendering, pumps input, drains the command bus, drains the stroke buffer, steps physics, handles snapshot exports, and draws each frame.

`scene_state.c` is the place to hook in new solvers: inject additional data into `SceneState`, update it inside `scene_apply_input`, respond to commands, and step it next to the fluid loop that already reads `AppConfig`.
