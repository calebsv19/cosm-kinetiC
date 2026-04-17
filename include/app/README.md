# `include/app/`

Interfaces that describe global configuration and scene orchestration.

- `app_config.h` – declares the `AppConfig` struct plus `app_config_default()`. Values here tune the entire simulation (window size, grid resolution, timestep clamps, solver constants) and are read by the renderer, physics solvers, and timing helpers.
- `scene_menu.h` – describes the SDL-based scene editor UI that lets the user pick presets and tweak grid resolution before the simulation boots.
- `scene_presets.h` – declares the emitter structs and accessors for built-in fluid presets (hotspots, jets, sinks).
- `scene_state.h` – defines the `SceneState` struct and its lifecycle helpers. It exposes functions to create/destroy the scene, apply `InputCommands`, respond to queued commands, manage brush samples/emitters, and export `.ps2d` snapshots.
- `sim_runtime_backend.h` – declares the extracted runtime-backend boundary that `SceneState` dispatches through for 2D and scaffolded 3D lanes.
- `sim_runtime_3d_domain.h` – declares the owned XYZ domain/container contract used by the 3D backend.
- `sim_runtime_emitter.h` – declares the backend-owned emitter taxonomy and 3D placement contract used to keep PSBU-4 emitter work off old 2D mask assumptions.
- `scene_controller.h` – declares the orchestration entry point that main calls to run the full SDL loop.
- `editor/` – scene editor interfaces split across canvas rendering (`scene_editor_canvas.h`), geometry/handle helpers (`scene_editor_canvas_geom.c` implementation), hit testing (`scene_editor_canvas_hit.c` implementation), input/panel wiring, and model helpers. Include these from this subdirectory when adding editor features.

Implementation lives under `src/app/`. Any new high-level system that needs to persist across frames should have its struct declared here and be owned by `SceneState`.
