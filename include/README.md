# `include/`

Public headers for every subsystem. The layout mirrors `src/` so it is easy to find the declaration that matches an implementation file.

## Files
- `timing.h` – declares the `FrameTimer` and helper routines the main loop uses to clamp delta time.

## Subdirectories
- `app/` – configuration structs and scene orchestration APIs.
- `command/` – shared command bus interfaces.
- `config/` – loaders that read JSON files into runtime structs.
- `input/` – definition of the `InputCommands` struct consumed by the rest of the program.
- `physics/` – math helpers plus the fluid, particle, smoke, soft-body, and rigid-body interfaces.
- `render/` – SDL renderer front-end.

When adding a new module, place its header alongside peers here, update the corresponding directory README, and keep implementation details confined to `src/`.
