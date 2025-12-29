# `src/app/editor`

Scene editor logic lives here. These files wrap the scene editor into a functional subset of the full scene control to allow scene modifications.

- `scene_editor.c` – the high-level scene editor logic: handles slot renaming, input routing, and ties the canvas/panel helpers together.
- `scene_editor_canvas.c` – render-only canvas helper (draws objects/imports/emitters, tooltips, preset title, boundary flows).
- `scene_editor_canvas_geom.c` – projection helpers, normalized ↔ pixel conversions, handle sizing, and object/import handle placement math.
- `scene_editor_canvas_hit.c` – hit collection for emitters/objects/imports/boundary edges plus emitter-handle hit tests.
- `scene_editor_widgets.c` – UI primitives (buttons, numeric fields) shared across the editor panel.

