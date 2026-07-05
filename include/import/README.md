# `include/import/`

Public import/projection contracts for retained runtime-scene and asset input.

- `runtime_scene_bridge.h` - retained runtime-scene bridge API.
- `runtime_mesh_preview_bridge.h` - runtime mesh preview-sidecar metadata and
  diagnostic bridge.
- `shape_import.h` - legacy shape import API.

Import headers should expose stable translation/projection contracts only.
Keep implementation details private in `src/import/` when a helper is not
needed across source lanes.
