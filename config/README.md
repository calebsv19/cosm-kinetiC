# `config/`

User-editable data files. `app.json` holds the authoritative runtime settings (window size, grid resolution, timestep, solver constants, and how many commands the controller drains per frame). The loader in `src/config/` reads this file at startup so you can tweak the simulation without recompiling.
