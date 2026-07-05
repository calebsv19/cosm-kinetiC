# `src/`

Implementation for the `physics_sim` / `kinetiC` runtime. Public headers live
under the mirrored `include/` tree unless a header is intentionally private to
one source lane.

## Files

- `main.c` - thin process entry. It delegates to
  `physics_sim_app_main(...)`; lifecycle ownership lives in
  `src/app/physics_sim_app_main.c` with the public wrapper declared in
  `include/physics_sim/physics_sim_app_main.h`.
- `timing.c` - small helper that clamps frame delta time via `FrameTimer`.

## Subdirectories

- `app/` - app shell, config defaults, menu/editor flows, retained-scene
  sessions, runtime controllers, sparse `3D` backend glue, Water/Wind mode
  routing, runtime mesh solver-role handling, structural mode controllers,
  headless quality profiles, loop policy, and app-local UI adapters.
- `command/` - command bus implementations used to relay actions from input to
  higher layers.
- `config/` - loaders that read `config/app.json` and populate `AppConfig`.
- `export/` - volume frame writers, render-frame output, Water heightfield
  sidecars, Wind projection frames, export path helpers, manifests, and
  `scene_bundle` handoff artifacts.
- `geo/` - geometry helpers used by app/runtime projection lanes.
- `import/` - retained runtime-scene intake, runtime-scene-to-solver
  projection, runtime mesh preview bridge, and legacy shape import support.
- `input/` - SDL event pump, stroke buffer utilities, and related input
  command helpers.
- `physics/` - numerical solver families for fluid, particles, rigid bodies,
  soft bodies, smoke/scalar fields, structural mechanics, and shared math.
- `render/` - SDL presentation renderer plus HUD/field/velocity/particle and
  retained-runtime overlay helpers. Renderer-free Wind diagnostic frames live
  in `src/export/`, not here.
- `tools/` - CLI and diagnostic tools, including `physics_sim_headless`,
  `physics_sim_job_runner`, trace/export tools, runtime-scene diagnostics, and
  shape asset tools.
- `ui/` - app-local UI support that is not owned by a narrower app sublane.

## Active Ownership Notes

- Water Basin runtime/export work is owned by the app/runtime and export lanes:
  `src/app/`, `src/export/water_surface_artifacts.*`, and the headless CLI.
- Wind solver/analyzer work spans app runtime/backend code, export-side Wind
  projection frames, retained-scene editor setup, and headless diagnostics.
- Runtime mesh behavior spans retained-scene import/projection, app runtime
  mesh obstacle/emitter handling, editor inspector readouts, and diagnostic
  tools.
- Detached jobs are CLI/tooling surfaces under `src/tools/cli/` plus generated
  status/progress artifacts under ignored runtime/build roots.
- Physics trio handoff boundaries are source-owned here only for PhysicsSim
  producer/export/runtime proof surfaces. LineDrawing/sCulpt authoring and
  RayTracing import/render review are separate program lanes.

When adding a new system, choose the narrowest existing subtree first, add or
update the matching header under `include/`, and update the nearest README.
