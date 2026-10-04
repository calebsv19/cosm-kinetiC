# Stationary obstacle pressure projection

The MAC core now supports a stationary cell-aligned rectangular solid in its
pressure projection. This is the first actual obstacle topology in this CFD
path, not an externally supplied surface-force calculation. It is deliberately
not yet a complete obstacle-flow simulation.

## Implemented behavior

`cfd_mac2d_set_obstacle` installs one half-open rectangle of solid cells at rest,
before time advances. X bounds must leave a fluid column on both sides; Y bounds
may touch the existing walls. Interior blocks and full-height baffles are covered.
The admitted rectangle leaves one connected fluid component under periodic X.
Multiple bodies, disconnected cavities, moving geometry, cut cells and STL
rasterization are not supported.

A velocity face is open only when both adjacent cells are fluid. The pressure
matrix, gradient correction and divergence all use that same topology. Solid
rows are excluded, and the pressure gauge averages only fluid cells. Blocked
normal velocities must already be exactly zero: projection rejects nonzero
blocked-face input rather than silently discarding unaccounted wall momentum.

Projection computes `obstacle_pressure_force_x_n` from neighboring fluid pressure
on the solid's exposed X faces. This is the reaction associated with the solved
pressure correction, in N. It excludes imposed mean-gradient forcing, viscous
traction and transport. It is not total drag or a drag coefficient. Pressure at
obstacle faces is represented by the neighboring fluid cell; no continuum
surface-pressure accuracy claim is made.

## Proof

`make test-cfd-mac2d-obstacle` exercises an interior block and a full-height baffle
at 8, 16 and 32 cells per axis. A prescribed nonuniform discrete pressure field
with a nonzero body load generates the initial face-gradient velocity. The
masked projection must recover that pressure and remove the gradient velocity.

Across all six cases:

- maximum physical-pressure error is below 1.7e-11 Pa;
- blocked-face normal leakage is exactly zero;
- body pressure reactions are deliberately nonzero (.221 to .653 N);
- pressure reaction plus fluid streamwise momentum-change rate balances within
  8e-16 N in the normal build;
- the independently assembled known-pressure load matches the measured load;
- adding a constant pressure gauge leaves the closed-body X load unchanged;
- projection energy does not increase and divergence satisfies the 1e-8 s^-1 gate;
- duplicate geometry and nonzero blocked-face velocity are rejected.

These are discrete operator/force-consistency tests, not flow-past-body drag
benchmarks or proof of continuum surface-stress convergence. Existing MAC core,
MAC agent and reduced-channel agent tests pass. The final obstacle test also
passes address/undefined-behavior sanitizers. Evidence is retained under
`build/s3-mac2d-obstacle-projection/`.

## Remaining boundary implementation

The [masked momentum extension](cfd_mac2d_obstacle_momentum.md) now advances
stationary obstacles with conservative donor fluxes, no-slip viscous ghosts and
substep-integrated pressure/viscous reactions. Complete timestep momentum and
leakage checks replace the earlier stepping rejection. Scene/MCP obstacle
authoring remains unavailable while geometry and physical force accuracy are
unqualified. Read the extension for forcing/dual-volume limitations.

Open boundaries still require separate inlet/outlet face storage and a compatible
pressure boundary operator. Their mass/momentum, reverse-flow and disturbance-exit
tests remain outstanding. This checkpoint grants no open-outlet or physical drag
acceptance. The default unobstructed channel path and desktop package remain in
their previous roles; no commit or package refresh is performed.
