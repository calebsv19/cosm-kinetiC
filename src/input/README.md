# `src/input/`

Translates raw SDL events into the higher-level `InputCommands` struct that the rest of the engine understands.

- `input.c` – gathers SDL events, sets the analog cursor state (`mouse_down`, `mouse_x`, `mouse_y`), and pushes discrete actions into the `CommandBus`:
  - `P` pauses, `C` clears the smoke field, `E` exports a snapshot.
  - `1` selects the high-density brush, `2` selects the pure-velocity brush. The selected mode is mirrored back via `InputCommands` so the stroke sampler can tag buffered samples correctly.
  - Overlay toggles: `V` (vorticity), `B` (pressure), `S` (velocity vectors), **Shift + S** (velocity-mode toggle between magnitude and fixed length), `L` (particle trails).
  - `kit_viz` path toggles: `K` (density path), `J` (velocity path), **Shift + V** (vorticity path), **Shift + B** (pressure path), **Shift + L** (particle path). These swap adapter-first vs legacy rendering paths without changing simulation state.
- `input_context.c` – stack-based input context manager that lets the menu, editor, and runtime push their own pointer/key handlers without rewiring the global input loop.
- `stroke_buffer.c` – dynamically resizable ring buffer used by the scene controller to keep a high-frequency history of cursor samples. This is what makes brush strokes appear continuous even when the simulation takes longer per frame.

Painting is now handled by the stroke sampler inside the scene controller, so this module only reports the current cursor state rather than streaming brush commands.

If you add bindings, extend `InputCommands` in `include/input/input.h`, update the polling logic here, and document the new behavior in this README so downstream modules know which signals exist.
