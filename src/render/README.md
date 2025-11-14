# `src/render/`

Everything related to presenting the simulation to the screen.

- `renderer_sdl.c` – initializes an SDL window/renderer/texture sized to the simulation grid, converts fluid density into an RGBA buffer each frame (normalizing by the current maximum density), scales the texture to the window, and swaps buffers. Also exposes shutdown helpers that the main loop calls on exit.

Future render paths (e.g., debug overlays, particle sprites, rigid-body wireframes) should either extend this file or live next to it, with their interfaces described in the paired header under `include/render/`.
