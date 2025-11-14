# `src/physics/particles/`

Tracer particle system implementation. Maintains a resizable array of particles, samples the fluid velocity field for advection, applies gravity, integrates positions, and culls dead particles. Future upgrades (interaction with rigid/soft bodies, rendering billboards) will build from this foundation.
