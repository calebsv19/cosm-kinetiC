# `src/render/`

Runtime presentation and overlay code lives here.

- `renderer_sdl.c` - initializes the SDL window/renderer/texture, converts the
  current simulation view into pixels, applies the optional blur pass, draws
  overlays, and swaps buffers.
- `field_overlay.c`, `velocity_overlay.c`, `particle_overlay.c`,
  `hud_overlay.c`, and `debug_draw_objects.c` - runtime HUD and diagnostic
  overlays for field, velocity, particle, object, and summary readouts.
- `retained_runtime_scene_overlay.c` - retained runtime-scene overlay drawing
  for loaded scene objects and runtime mesh preview/readout support.

Renderer ownership boundaries:

- This lane owns presented SDL drawing and runtime overlays.
- Renderer-free Wind diagnostic frames are exported from `src/export/`; they
  are proof/debug artifacts, not the interactive renderer.
- Headless volume and Water sidecar artifacts are export-lane outputs.
- Source-run first-frame visual proof should use the explicit proof target or
  harness for the current pass rather than adding always-on renderer behavior.

Future render paths should use a narrow file next to the existing overlay or
renderer module, with public interfaces described under `include/render/` only
when another lane needs to call them.
