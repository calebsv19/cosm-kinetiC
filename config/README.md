# `config/`

User-editable data files. `app.json` holds the authoritative runtime settings (window size, grid resolution, timestep, solver constants, and how many commands the controller drains per frame). The loader in `src/config/` reads this file at startup so you can tweak the simulation without recompiling.

Newly exposed keys:
- `input.stroke_sample_rate` / `input.stroke_spacing` – control how frequently brush samples are queued and how densely they fill gaps.
- `emitters.density_multiplier`, `emitters.velocity_multiplier`, `emitters.sink_multiplier` – global scales applied to preset emitters so you can quickly boost or attenuate effects without editing the presets themselves.
- `render.blur_enabled` – toggles the separable blur pass used when upscaling low-res grids.
- `fluid.solver_iterations` – lets you tighten or relax the pressure solver without recompiling.
- `headless.*` – enable the headless batch runner, pick the target preset/quality index, set the frame budget, output directory, and whether to present frames while running.
