# `include/app/`

Interfaces that describe global configuration and scene orchestration.

- `app_config.h` – declares the `AppConfig` struct plus `app_config_default()`. Values here tune the entire simulation (window size, grid resolution, timestep clamps, solver constants) and are read by the renderer, physics solvers, and timing helpers.
- `data_paths.h` – declares path-resolution helpers for input/output/runtime storage. It now also exposes `physics_sim_runtime_scene_catalog_roots(...)` so menu/editor scene-library discovery can stay centralized.
- `physics_sim_file_helpers.h` – declares app-local non-destructive path/text helpers for support surfaces such as detached jobs and headless bundle IO. Destructive cleanup and overwrite policy are intentionally not part of this contract.
- `physics_sim_json_helpers.h` – declares app-local JSON string escaping for manual artifact/status writers without centralizing artifact schemas.
- `physics_sim_cli_helpers.h` – declares app-local CLI value/scalar parsing helpers for support tools without introducing a generic option parser.
- `physics_sim_diagnostic_helpers.h` – declares app-local diagnostic string helpers for detached-job/headless support surfaces without merging UI status, stderr logging, or structured status schemas.
- `scene_menu.h` – describes the SDL-based scene editor UI that lets the user pick presets and tweak grid resolution before the simulation boots.
- `scene_presets.h` – declares the emitter structs and accessors for built-in fluid presets (hotspots, jets, sinks).
- `scene_state.h` – defines the `SceneState` struct and its lifecycle helpers. It exposes functions to create/destroy the scene, apply `InputCommands`, respond to queued commands, manage brush samples/emitters, and export `.ps2d` snapshots.
- `sim_runtime_backend.h` – declares the extracted runtime-backend boundary that `SceneState` dispatches through for 2D and scaffolded 3D lanes.
- `sim_runtime_3d_domain.h` – declares the owned XYZ domain/container contract used by the 3D backend.
- `sim_runtime_emitter.h` – declares the backend-owned emitter taxonomy and 3D placement contract used to keep PSBU-4 emitter work off old 2D mask assumptions.
- `scene_controller.h` – declares the orchestration entry point that main calls to run the full SDL loop.
- `scene_loop_policy.h` – declares the shared loop wait-policy contract used by menu/runtime loops to choose wait-vs-ramp behavior.
- `scene_loop_diag.h` – declares the loop diagnostics sink used for schema-locked `LoopDiag` output.
- `editor/` – scene editor interfaces split across canvas rendering (`scene_editor_canvas.h`), retained-scene document/session/library/view state (`scene_editor_retained_document.h`, `scene_editor_session.h`, `scene_editor_scene_library.h`, `scene_editor_viewport.h`), geometry/handle helpers, hit testing, input/panel wiring, and model helpers. Include these from this subdirectory when adding editor features.

Implementation lives under `src/app/`. Any new high-level system that needs to persist across frames should have its struct declared here and be owned by `SceneState`.
