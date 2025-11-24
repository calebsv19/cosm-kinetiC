# `src/render/`

Everything related to presenting the simulation to the screen.

- `renderer_sdl.c` – initializes an SDL window/renderer/texture sized to the simulation grid, converts fluid density into an RGBA buffer each frame (normalizing by the current maximum density), optionally runs a separable blur (controlled by quality/`AppConfig`), draws TimerHUD overlays, scales the texture to the window, and swaps buffers. It also manages HUD + overlay modules (pressure/vorticity/velocity vectors/particle trails/object outlines) and exposes shutdown helpers plus a capture path used by the headless exporter. Overlay modules live alongside this file (`field_overlay.c`, `velocity_overlay.c`, `particle_overlay.c`, `hud_overlay.c`, `debug_draw_objects.c`) and all read from the cached obstacle mask/distance map supplied by `SceneState`, so they remain cheap even on large grids.

Future render paths (e.g., debug overlays, particle sprites, rigid-body wireframes) should either extend this file or live next to it, with their interfaces described in the paired header under `include/render/`.
