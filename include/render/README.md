# `include/render/`

Renderer interfaces exposed to the rest of the program.

- `renderer_sdl.h` – declares initialization, shutdown, and draw functions for the SDL renderer implemented under `src/render/`. The renderer depends on `SceneState` so it can read the current `Fluid2D` buffers when filling the texture each frame.

Future rendering backends (e.g., OpenGL or debug overlays) should add additional headers here and keep the implementations inside `src/render/`.
