# `src/app/editor`

Scene editor logic lives here. This lane owns retained-scene selection,
editing, viewport, panel/inspector, writeback, and app-local editor state. It
does not own solver stepping or export artifacts.

Object-body hover/click routes projected object origins through shared
`core_screen_pick >= 0.1.0`, including deterministic ranked overlap results for
the existing repeat-click cycle. PhysicsSim still owns projection, handles,
imports, emitters, boundaries, hit-stack arbitration, drag policy, and drawing.

- `scene_editor.c` – the high-level scene editor logic: handles slot renaming, input routing, and ties the canvas/panel helpers together.
- `scene_editor_session.h` / `scene_editor_session.c` – app-local editor-session seam above retained runtime-scene intake. Owns canonical-scene bootstrap plus mirrored legacy editor-selection state while legacy `FluidScenePreset` editing remains the compatibility adapter.
- `scene_editor_scene_library.h` / `scene_editor_scene_library.c` – app-local scene-library split between 2D preset entries and retained 3D runtime-scene entries. Owns multi-root retained-scene discovery, selection state, scene-id loading, active-scene tracking, and directory-based `scene_runtime.json` support.
- `scene_editor_state.c` – central editor state host that refreshes the scene library, session, viewport lanes, and selected runtime-mesh diagnostic cache together as mode/path state changes.
- `scene_editor_viewport.h` / `scene_editor_viewport.c` – editor framing/orbit/zoom policy. Large retained scenes now use scene-relative distance limits so the `3D` camera can zoom far enough out to inspect oversized bounds.
- `scene_editor_input_common.h` / `scene_editor_input_common.c` – shared post-save and cross-path helpers, including scene-library refresh after retained-scene writes.
- `scene_editor_pane_host.h` / `scene_editor_pane_host.c` – app-local pane-host wrapper that assigns left/center/right editor lanes through shared `core_pane` split solve semantics.
- `scene_editor_panel*.c` - right/left panel rendering and interaction,
  including Scene Physics, Object Physics, Source/Emitter, and diagnostics
  grouping. Keep long mesh/cache/Wind readouts in inspector/diagnostic
  surfaces rather than the always-on HUD.
- `scene_editor_wind_setup.c` - retained Wind setup and writeback helpers for
  `extensions.physics_sim.wind_tunnel`.
- `scene_editor_canvas.c` – render-only canvas helper (draws objects/imports/emitters, tooltips, preset title, boundary flows).
- `scene_editor_canvas_geom.c` – projection helpers, normalized ↔ pixel conversions, handle sizing, and object/import handle placement math.
- `scene_editor_canvas_hit.c` – hit collection for emitters/objects/imports/boundary edges plus emitter-handle hit tests.
- `scene_editor_widgets.c` – UI primitives (buttons, numeric fields) shared across the editor panel.

Focused contract coverage for current retained-scene/editor behavior lives in:

- `tests/scene_editor_scene_library_contract_test.c`
- `tests/scene_editor_viewport_contract_test.c`
- `tests/scene_editor_wind_setup_contract_test.c`
- `tests/scene_editor_pane_host_contract_test.c`
