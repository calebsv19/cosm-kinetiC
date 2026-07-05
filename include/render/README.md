# `include/render/`

Renderer and overlay interfaces exposed to the rest of the program.

- `renderer_sdl.h` - SDL renderer lifecycle, draw, shutdown, and capture
  functions implemented under `src/render/`.
- overlay headers - runtime HUD, field, velocity, particle, object, retained
  runtime-scene overlay, and shared render constants used by app/runtime code.

Keep renderer-free export diagnostics in `include/export/` / `src/export/`.
Add headers here only for presented runtime rendering or overlay interfaces
that another source lane must call.
