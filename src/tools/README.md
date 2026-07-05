# `src/tools/`

Command-line and diagnostic tools live here.

- `cli/physics_sim_headless.c` - direct retained-scene, Water, Wind, volume,
  and render-frame headless runner.
- `cli/physics_sim_job_runner.c` - detached submit/status/cancel runner and
  job-status surface.
- `cli/physics_trace_tool.c`, `cli/vf2d_pack_tool.c`, and
  `cli/vf2d_dataset_tool.c` - trace, pack, and dataset export tooling.
- `cli/runtime_scene_emitter_diag_tool.c` - runtime-scene, runtime mesh,
  emitter, footprint, and Wind projected-area diagnostic summaries.
- `cli/shape_*` plus `ShapeLib/` - shape import, flattening, validation, and
  ShapeAsset conversion tools.

Tools may call app/import/export contracts, but they should keep product
runtime policy in the owning source lane. Generated outputs belong under
ignored `tmp/`, `build/`, `export/`, or caller-selected output roots.
