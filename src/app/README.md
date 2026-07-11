# `src/app/`

App shell and runtime orchestration live here. This lane adapts solver,
renderer, editor, menu, headless, and retained-scene contracts into the
interactive `kinetiC` program.

## Top-Level App Shell

- `physics_sim_app_main.c` - lifecycle wrapper called by `src/main.c`.
- `app_config.c` - default `AppConfig` values for window, grid, timestep,
  solver tuning, quality profile, render blur, and headless defaults.
- `data_paths.c` - runtime/input/output path resolution and retained-scene
  catalog roots for the configured `Input Root`.
- `physics_sim_file_helpers.c` - app-local non-destructive path/text helpers
  used by detached jobs and headless bundle support. Destructive cleanup and
  overwrite policy stay with the owning CLI/runtime surface.
- `physics_sim_json_helpers.c` - app-local JSON string escaping helper for
  manual artifact/status writers. Artifact schemas remain owned by their
  writer modules.
- `platform/physics_sim_file_picker.*` - app-local host chooser adapter for
  Input Root, Output Root, and Atmospheric Warm Start. macOS uses
  `osascript`; Linux tries `zenity` then `kdialog`, while the menu retains
  manual path entry when no desktop picker is available.
- `platform/physics_sim_path_opener.*` - app-local directory opener for the
  Output Root `Show` action. macOS uses `open`; Linux tries `xdg-open` then
  `gio open`, preserving an in-app status fallback when no opener is usable.
- `physics_sim_cli_helpers.c` - app-local CLI value/scalar parsing helpers for
  support tools. It does not define a generic parser or own tool usage text.
- `physics_sim_diagnostic_helpers.c` - app-local diagnostic string helpers for
  detached-job/headless support surfaces. UI status, stderr logging, and
  structured artifact status remain owned by their surfaces.
- `physics_sim_job_runner_status.c` - detached job status projection. It may
  read `run_progress.json`, but headless progress remains a separate
  headless-owned output contract; use the named progress-status projection
  helper rather than merging schemas.
- `quality_profiles.c` - built-in Preview/Balanced/High/Deep profiles.
- `scene_loop_policy.c` / `scene_loop_diag.c` - shared wait policy and
  schema-locked loop diagnostics for menu/runtime/editor-like states.

## Menu, Presets, And Persistence

- `scene_menu.c` and `menu/` - menu shell, editable slots, quality/path
  controls, headless batch controls, and mode-specific settings panels.
- `preset_io.c` - custom preset persistence and compatibility loading.
- `scene_presets.c` - built-in emitter/preset catalog.
- `sim_modes/` - mode-specific routing helpers.
- `ui/` - app-local UI adapters, including the `kit_ui` button bridge.

## Runtime Controllers

- `scene_controller.c` - SDL/runtime loop with explicit IR1 input phases and
  RS1 update/render phases. It owns frame orchestration, not solver policy.
- `scene_state.c` - legacy `2D` scene state host and compatibility bridge for
  brush samples, preset emitters, obstacle masks, command responses, and
  grouped retained-runtime view UI state.
- `sim_runtime_backend*.c`, `sim_runtime_3d_*`, `sim_runtime_emitter.c`, and
  `sim_runtime_obstacle.c` - extracted `2D` / sparse `3D` backend, runtime
  mesh, emitter, obstacle, Wind, and reporting seams. New `3D` runtime work
  should start in the narrow backend file that owns the selected behavior.
  Obstacle cache/dirty state remains backend-owned; use the internal
  `backend_3d_scaffold_mark_obstacle_*` helpers rather than open-coding dirty
  flag bundles.
- `sim_runtime_mesh_diagnostics.c` - selected runtime-mesh diagnostic collector
  for path resolution, preview/runtime metadata, solver role, voxel footprint,
  BVH/cache fallback, domain overlap, bounds volume, and Wind projected-area
  readouts. Keep wording changes tied to this owner or the import bridge that
  originates the diagnostic; do not adjust voxelization/cache behavior just to
  rename a message.
- Water mode currently uses app/headless runtime control plus export-side
  sidecars under `src/export/`; do not treat it as ordinary smoke density.

## Editor And Retained Scene Lanes

- `editor/` - retained-scene editor, scene library, viewport, panel/inspector,
  writeback, runtime mesh readouts, and Wind setup controls. See
  `editor/README.md`.
- `structural/` - dedicated structural mode controller, solver UI, editor, and
  render helpers. It shares the top-level menu entry but not the fluid
  headless/export runtime path.
- `atmospheric/` - atmospheric procedural field initialization and warm-start
  support used by standalone Atmospheric modes and optional `3D` presets.

## Ownership Notes

- Headless CLI entry points live under `src/tools/cli/`, but they call into app
  runtime/headless contracts here.
- Export artifacts, Water sidecars, Wind projection frames, and trio
  `scene_bundle` handoffs live under `src/export/`.
- Retained runtime-scene import/projection lives under `src/import/`; app code
  consumes those projected runtime documents.
- Menu settings draft apply/save/restore/reset flows route through
  `menu_settings_input.c` and its runtime binding; direct path/headless toggle
  config edits remain separate menu-shell mutations.
- GUI status and detailed diagnostics stay separated by surface. Keep
  `menu_set_status(...)` messages concise and user-facing; keep editor
  Apply/Save summaries in editor status cards; route long selected-object,
  runtime-mesh, and Wind detail through inspector/readout lanes; route
  launcher, headless, detached-job, and renderer setup detail through their
  owning logs, artifacts, or stderr contracts.
- Detailed current state belongs in `physics_sim/docs/current_truth.md` and
  private active plans. Keep this README focused on source ownership.
