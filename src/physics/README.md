# `src/physics/`

Root for every solver. Each subsystem lives in its own folder so we can scale from simple 2D demos to the future 3D pipeline without growing monolithic files.

- `fluid2d/` – density + velocity grid plus stepping logic (diffusion, advection, damping) driven by `AppConfig`. The solver now enforces scene-driven solid masks before/after every major phase and exposes the cached mask/distance arrays so overlays and particles can reuse the same boundary data cheaply.
- `particles/` – tracer particle pool that can read from the fluid and rigid worlds.
- `rigid/` – minimal rigid-body integrator with collisions against the ground plane and other circles.
- `soft/` – placeholder for deformable bodies; currently just alloc/free + linear integration.
- `smoke/` – represents scalar smoke density fields that can later be exported to the renderer.
- `math/` – vector and math helpers (mostly header-only right now) so every solver can share primitives.

Every folder has its own README plus headers housed under `include/physics/...`. When creating new solvers (pressure, collision primitives, 3D extensions) follow this pattern: put implementation here, declarations in the mirrored include folder, and document the module responsibilities.
