# `src/import/`

Runtime-scene and asset intake lives here.

- `runtime_scene_bridge.c` - retained runtime-scene bridge from authored JSON
  scene documents into PhysicsSim runtime data.
- `runtime_scene_solver_projection*.c` - projection from retained scene
  objects, emitters, domains, atmospheric initial state, and solver metadata
  into app/runtime solver inputs.
- `runtime_mesh_preview_bridge.c` - shared preview-sidecar bridge for runtime
  mesh metadata and diagnostics; solver truth still comes from authoritative
  runtime mesh geometry.
- `shape_import.c` - legacy shape import support for older 2D/object lanes.

This lane translates external/authored scene data into PhysicsSim-owned
runtime inputs. It does not own solver stepping, export artifacts, or
RayTracing-side rendering review.

Runtime scene / runtime mesh path boundary:

- Runtime-scene JSON is trusted local authored input unless a separate
  worker-safe bundle stages the scene and asset payloads explicitly.
- Runtime mesh paths may resolve from explicit absolute paths, paths relative
  to the runtime-scene directory, default `assets/mesh_assets/` /
  `mesh_assets/` locations, or migrated Desktop references. On Linux, legacy
  Desktop references recover in order from
  `$XDG_DATA_HOME/PhysicsSim/stls/`, `XDG_DESKTOP_DIR/stls/` from
  `user-dirs.dirs`, then `$HOME/Desktop/stls/` for compatibility.
- Absolute and recovered runtime mesh paths are valid local-authoring
  conveniences, not portable job-bundle guarantees.
- Worker or cross-host handoff flows must package the runtime scene, runtime
  mesh documents, preview sidecars, and run config together before treating the
  payload as portable.
- The import bridge only reads and classifies these paths. It should not
  create, delete, overwrite, download, or extract runtime mesh payloads.

Diagnostic wording boundary:

- Import diagnostics should identify the authored/runtime-scene input problem
  and the relevant path or object reference when available.
- Runtime-mesh preview diagnostics own path-resolution and preview-sidecar
  state such as missing scene JSON, missing objects, unresolved runtime mesh
  paths, preview probe state, and recovered runtime paths.
- Solver footprint, BVH/cache fallback, domain overlap, and Wind projected-area
  wording belongs with `src/app/sim_runtime_mesh_diagnostics.c` and the runtime
  mesh obstacle proxy, not the import bridge.
- Do not change voxelization, prepared-cache behavior, live-watch behavior, or
  Wind inspector UI from a wording-only diagnostics pass.
