# `src/app/editor`

Scene editor logic lives here. These files wrap the scene editor into a functional subset of the full scene control to allow scene modifications 


- `editor/scene_editor.c` – the high-level scene editor logic: handles slot renaming, input routing, and ties the canvas/panel helpers together.
- `editor/scene_editor_canvas.c` – canvas helper that draws emitters, tooltips, and the preset title, plus the hit-testing/projection math.
- `editor/scene_editor_widgets.c` – UI primitives (buttons, numeric fields) shared across the editor panel.


