# `include/physics/`

Public APIs for every physics-oriented subsystem. Each subfolder mirrors the implementation living under `src/physics/`.

- `math/` – inline vector helpers and, later, gradient/divergence kernels.
- `fluid2d/` – `Fluid2D` grid structure plus stepping/brush helpers.
- `particles/` – tracer particle pool declarations.
- `rigid/` – rigid-body structs and stepping routines.
- `soft/` – soft-body nodes and integration hooks.
- `smoke/` – smoke density field descriptions for volumetric exports.

Headers here should stay lightweight and dependency-free beyond other headers in this folder (plus `app/app_config.h`). That keeps solvers composable and avoids circular includes.
