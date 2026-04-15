# `src/app/editor`

Scene editor logic lives here. These files wrap the scene editor into a functional subset of the full scene control to allow scene modifications.

- `scene_editor.c` – the high-level scene editor logic: handles slot renaming, input routing, and ties the canvas/panel helpers together.
- `scene_editor_session.h` / `scene_editor_session.c` – app-local editor-session seam above retained runtime-scene intake. Owns canonical-scene bootstrap plus mirrored legacy editor-selection state while legacy `FluidScenePreset` editing remains the compatibility adapter.
- `scene_editor_scene_library.h` / `scene_editor_scene_library.c` – app-local scene-library split between 2D preset entries and retained 3D runtime-scene entries. Owns library discovery/selection state ahead of the later mode-aware open/save UI.
- `scene_editor_pane_host.h` / `scene_editor_pane_host.c` – app-local pane-host wrapper that assigns left/center/right editor lanes through shared `core_pane` split solve semantics.
- `scene_editor_canvas.c` – render-only canvas helper (draws objects/imports/emitters, tooltips, preset title, boundary flows).
- `scene_editor_canvas_geom.c` – projection helpers, normalized ↔ pixel conversions, handle sizing, and object/import handle placement math.
- `scene_editor_canvas_hit.c` – hit collection for emitters/objects/imports/boundary edges plus emitter-handle hit tests.
- `scene_editor_widgets.c` – UI primitives (buttons, numeric fields) shared across the editor panel.
