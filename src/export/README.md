# `src/export/`

Export and proof artifact writers live here.

- `volume_frames.c` / `volume_frames_vf3d.c` - legacy planar VF2D/VFHD and
  authoritative volumetric VF3D/VF3H frame output, including manifests and
  `scene_bundle` metadata.
- `water_surface_artifacts.c` - Water Basin `water_manifest_v1.json` and
  per-frame `water_surface_*.json` heightfield sidecars.
- `wind_projection_frames.c` - renderer-free Wind analyzer/projection BMP
  frames used by headless visual diagnostics.
- `render_frames.c` - render-frame output helpers for headless/export review
  paths.
- `export_paths.c` - export path helpers shared by output writers.

This lane owns generated artifacts and handoff metadata. Runtime behavior
should stay in `src/app/` or `src/physics/`; import/consumer behavior belongs
in `src/import/` or downstream programs such as `ray_tracing`.
