# `include/export/`

Public export contracts for PhysicsSim-generated artifacts.

- `volume_frames.h` - VF2D/VFHD and VF3D/VF3H frame/manifest interfaces.
- `water_surface_artifacts.h` - Water Basin sidecar writer contract.
- `wind_projection_frames.h` - Wind analyzer/projection frame writer contract.
- `render_frames.h` - render-frame output helper contract.
- `export_paths.h` - path helpers for generated artifact roots.

Use these headers from app/headless/tooling code when producing artifacts.
Consumer/import behavior belongs under `include/import/` or downstream
programs.
