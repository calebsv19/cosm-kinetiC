# `src/physics/particles/`

Tracer particle system implementation. Maintains a resizable array of particles, samples the fluid velocity field for advection, applies gravity, integrates positions, and culls dead particles. The current particle/fluid bridge intentionally routes through the shared `fluid2d` grid-space sampler and then applies a local compatibility blend, rather than claiming a fully cleaned world-units contract yet. Future upgrades (interaction with rigid/soft bodies, rendering billboards) will build from this foundation.
