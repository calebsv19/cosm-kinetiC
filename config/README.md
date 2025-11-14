# `config/`

User-editable data files. `app.json` holds the authoritative runtime settings (window size, grid resolution, timestep, solver constants, and how many commands the controller drains per frame). The loader in `src/config/` reads this file at startup so you can tweak the simulation without recompiling.

Newly exposed keys:
- `input.stroke_sample_rate` / `input.stroke_spacing` – control how frequently brush samples are queued and how densely they fill gaps.
- `emitters.density_multiplier`, `emitters.velocity_multiplier`, `emitters.sink_multiplier` – global scales applied to preset emitters so you can quickly boost or attenuate effects without editing the presets themselves.
