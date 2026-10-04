# Stationary obstacle momentum evolution

The masked MAC core can now advance complete timesteps around a stationary
rectangular solid. This extends the earlier pressure-only obstacle operator with
viscous wall treatment and conservative donor-cell transport. The scene/agent
obstacle authoring path remains unavailable pending geometry/force accuracy
qualification; this implementation is exercised directly through the core API.

## Discrete boundary model

Only velocity faces joining two fluid cells evolve. Blocked normal velocities
remain exactly zero, and illegal nonzero blocked-face input is rejected before a
step changes the state. Advective flux between adjacent active staggered dual
volumes is shared with opposite signs. Flux through a blocked dual boundary is
zero. The masked path deliberately uses donor-cell flux, including in diagnostic
builds that select a different unmasked transport scheme.

Normal-component viscous neighbors at blocked faces use zero velocity.
Tangential neighbors across a solid boundary use reflected `-u` or `-v` ghosts,
placing zero tangential velocity at the intervening half-cell wall. Outer Y walls
retain their prescribed tangential velocity, but force accounting omits portions
covered by the solid. Corners use this stair-step dual-volume closure; it is not
a cut-cell method and has no proven continuum traction accuracy at corners.

Pressure projection still uses exactly the same fluid/solid connectivity as the
velocity correction. Solid sample locations return zero velocity and solid=1.
There is no moving-body dynamics, body motion, open outlet or STL geometry.

## Reactions and balance

The solver accumulates obstacle viscous reaction directly from the missing-neighbor
viscous terms in each momentum update. It accumulates pressure reaction from each
projection. Substep impulses are divided by the outer timestep to report:

- `obstacle_viscous_force_x_n`;
- `obstacle_mean_pressure_force_x_n`;
- `outer_wall_force_on_fluid_x_n`;
- `mean_drive_force_x_n`.

The older `obstacle_pressure_force_x_n` remains the final projection's pressure
reaction, not the tick average. The streamwise residual includes the integrated
obstacle reaction, outer-wall reaction and applied forcing. Internal advective
and viscous fluxes cancel in this discrete budget.

The imposed G/rho acceleration acts on active X-velocity degrees of freedom.
Its reported force uses those dual-volume weights; it is NOT automatically
G times the entire fluid-cell volume. In particular, a finite-resolution baffle
has adjacent blocked velocity faces. Pressure reaction is from the periodic
correction, not a reconstructed total affine pressure integrated on a continuum
body. Do not interpret these discrete reactions as qualified physical drag or
compare them directly to a free-stream drag coefficient.

## Verification

`make test-cfd-mac2d-obstacle-step` exercises rest, forward/reverse forcing,
interior blocks, full-height baffles and timestep/grid sensitivity. Each run
advances to 2 s. An independent cell-centred momentum integral reconstructs
velocity from the bounding faces and compares its change to the accumulated
forces. Tests require residual <1e-9 N, divergence <1e-8 s^-1 and exactly zero
normal leakage. Flow around the block must be nonzero; full-height baffle flow
must stay below 1e-8 m/s. After forcing is removed from the block cases, energy
must decrease without a tick-to-tick increase beyond roundoff tolerance.

Measured forward block reactions at T=2 s:

| Grid | Pressure reaction N | Viscous reaction N | Sum N |
|---|---:|---:|---:|
| 16² | .0338641 | .0209967 | .0548608 |
| 32² | .0367634 | .0197300 | .0564934 |
| 64² | .0386004 | .0186921 | .0572925 |

Successive force changes are about 2.98% then 1.41% relative to the coarser
value; this is sensitivity evidence, not acceptance against a physical reference.
At 32², halving dt from .005 to .0025 s changes the combined force by less than
7e-12 N at T=2 s. This nearly stationary endpoint is not a temporal-order test.
Normal leakage remains exactly zero; maximum divergence is below 4.1e-12 s^-1
and independent momentum residual below 8.5e-14 N across the reported runs.
The full-height baffle pressure reactions are .06875, .071875 and .0734375 N.
For this geometry, G times the continuum fluid volume is .075 N. The deficit
halves under grid doubling and follows the omitted boundary-adjacent velocity
volume. It is concrete evidence of first-order forcing/surface representation
error, even though the internal momentum residual is tiny. Repairing near-wall
volume weights and pressure reconstruction is required before physical force
qualification; do not hide this deficit by reporting the residual alone.
Full-height baffles stay below 1e-13 m/s. Reversed forcing reverses the final
pressure and viscous reactions. Unforced follow-on decay passes the energy screen.

See `build/s3-mac2d-obstacle-momentum/` for logs, measured sensitivity and source
provenance. The old pressure projection, channel core, surface-force calibration
and agent channel tests remain regression gates. This proves a consistent masked
numerical update, not experimental force accuracy or a formal spatial order.

## Remaining qualification

Before exposing obstacles through MCP, define geometry and forcing semantics
that remain consistent across resolution; validate flat-wall shear against a
known solution and quantify corner/traction errors. Extend physical pressure and
viscous surface integration independently of this internal impulse accounting.
Then add separate inlet/outlet faces and pressure boundary conditions, with
mass/momentum, backflow and domain-length tests. No open-outlet or drag acceptance
is implied here. Desktop packaging and commit state are unchanged.

Address/undefined-behavior sanitizers pass the eight bounded 16/32-grid cases.
The initial full-grid sanitizer run was stopped for runtime cost; 64-grid
numerical verification passed in the normal build, not under sanitizers.

The [surface-pressure correction and CFD assessment](cfd_boundary_pressure_correction.md)
now reconstructs pressure at obstacle walls and recovers the .075 N baffle
reference at all tested grids. Discrete reactions and unresolved boundary-volume
drive remain separate; arbitrary drag and open outlets are still unqualified.
This remains a 2D CFD core; the existing 3D Wind path is separate.
