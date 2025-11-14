# `src/physics/fluid2d/`

Actual implementation of the 2D fluid grid:
- Allocates density/velocity arrays (plus scratch buffers) and exposes brush helpers.
- Implements a semi-Lagrangian “stable fluids” step: Gauss-Seidel diffusion, projection, advection, buoyancy, and exponential density decay tuned by `AppConfig`.
- This module is now the staging ground for further Navier-Stokes upgrades (obstacles, vorticity confinement, coupling with rigid bodies, etc.).
