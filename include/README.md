# `include/`

Public headers for `physics_sim` / `kinetiC`. The layout mirrors `src/` for
subsystems that expose cross-lane interfaces. Implementation-private helpers
should stay in `src/` beside their owning `.c` files.

## Files

- `timing.h` - declares the `FrameTimer` and helper routines the main loop uses
  to clamp delta time.

## Subdirectories

- `physics_sim/` - product/program wrapper headers. The current process entry
  delegates through `physics_sim_app_main(...)`.
- `app/` - app shell, configuration structs, menu/editor interfaces, runtime
  backend contracts, loop policy, retained-scene editor headers, structural
  controllers, and app-local UI adapter interfaces.
- `command/` - command bus interfaces.
- `config/` - JSON config loader interfaces.
- `export/` - public export contracts for volume frames, render frames, Wind
  projection frames, and export path helpers.
- `geo/` - geometry helper interfaces shared by runtime/editor projection
  lanes.
- `import/` - retained runtime-scene bridge, runtime mesh preview bridge, and
  shape import interfaces.
- `input/` - `InputCommands` and SDL/input intake helpers consumed by the app
  layer.
- `physics/` - solver-facing APIs for fluid, particle, rigid, soft, smoke,
  structural, and math lanes.
- `render/` - SDL renderer, runtime HUD/field/velocity/particle overlays,
  retained runtime-scene overlay, and shared render constants.
- `ui/` - local UI helper interfaces not owned by a narrower app sublane.

When adding a new module, keep public headers here only when another lane needs
the interface. Otherwise prefer a private header next to the implementation in
`src/`. Update the nearest README whenever a new source/header lane becomes a
stable ownership boundary.
