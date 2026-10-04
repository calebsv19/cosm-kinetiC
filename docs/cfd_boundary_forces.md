# Boundary traction and momentum qualification foundation

This slice implements physical surface traction and signed advective momentum
accounting, calibrates them independently, and attaches measured channel wall
loads to MAC agent inspection. It does not add obstacle geometry or open outlets.

## Implemented contract

`cfd_surface_flux` takes SI density, dynamic viscosity, physical pressure, area,
unit normal, velocity, surface velocity and velocity gradient. The normal points
OUT of the fluid control volume; gradient[i][j] is du_i/dx_j. For incompressible
Newtonian flow it returns separately:

- pressure force on fluid: `-p n A`;
- viscous force on fluid: `mu (grad u + grad u^T) n A`;
- outward mass: `rho ((u-u_surface) dot n) A`;
- outward momentum: outward mass times absolute fluid velocity.

Fluid-on-body reaction is minus the pressure/viscous traction on fluid. Advective
momentum is not part of that reaction; it belongs in the control-volume balance.
The stationary-volume check is `d(momentum)/dt + outward momentum = traction +
body forcing`. Nonunit normals, nonfinite values, invalid areas and invalid fluid
parameters are rejected. Output is assigned only on success. This constitutive
contract is incompressible; no compressible bulk-viscosity term is implemented.

## Numerical calibration

`make test-cfd-surface-force` checks:

- pressure sign, full symmetric viscous stress (including cross derivatives),
  SI scaling, moving-surface relative flux and signed backflow;
- a closed rectangular body under a manufactured linear pressure field:
  net fluid-on-body force equals minus body volume times pressure gradient;
- pressure-gauge invariance of that closed-body integral;
- a solved 64-cell reduced channel through 800 startup ticks for both forward
  and reversed pressure gradients, assembling end-face pressure/momentum fluxes
  and wall stresses independently of the solver's own balance diagnostic.

The transient channel surface-balance residual is below 1.992e-15 N. Each wall's
steady signed fluid-on-wall load is +.05 N for G=.1 Pa/m and -.05 N when reversed.
The total imposed pressure drive is +/- .1 N. Mass and paired end-face momentum
fluxes cancel. These end-face fields are fully developed, prescribed-pressure
reductions; this is NOT a pressure-outlet solver test. The closed-body fixture
calibrates integration of supplied stresses; it does NOT solve flow around a box.
Address/undefined-behavior sanitizers and both channel agent suites pass.

## Agent integration

MAC snapshots now include `boundary_force_budget`:

- `bottom_fluid_on_wall_shear_force_x_n`, `top_fluid_on_wall_shear_force_x_n`;
- `mean_pressure_drive_force_x_n`;
- `instantaneous_drive_plus_wall_force_x_n`;
- `periodic_cut_outward_advective_momentum_x_n`;
- explicit wall-shear, obstacle, outlet and drag qualification status strings.

Wall loads use the actual X-averaged numerical near-wall gradients and the new
traction integrator. They do not include unqualified obstacle pressure loads.
The instantaneous force sum is NOT a transient balance residual: that requires
the existing substep-integrated momentum diagnostic. Mean pressure drive is an
equivalent force representation; do not count it again as both body force and
end-face pressure traction. Periodic cuts cancel by construction and provide no
evidence of open-outlet accuracy. Obstacle and outlet status remains unsupported;
drag remains not_qualified. Native UI and desktop package are not refreshed.

## Remaining implementation and acceptance gates

1. **Stationary grid-aligned obstacle:** fluid/solid topology and face apertures
   must enter both pressure divergence/gradient and momentum transport. Measure
   wall reaction from the same flux/stress used in the update. Verify no leakage,
   pressure gauge invariance, hydrostatic/constant-pressure cancellation and
   fluid-plus-body momentum. Start with an aligned plate/block before cut cells
   or STL stair-stepping, so geometry error can be isolated.
2. **Open inlet/outlet:** distinct boundary face storage and a pressure boundary
   operator must replace periodic identification. Specify inlet velocity and
   outlet pressure consistently; expose signed boundary mass/momentum and reverse
   flow. Recover the channel solution, then test disturbance exit, domain-length
   sensitivity and backflow. A copied velocity or converged Poisson residual is
   insufficient. OpenFOAM's [pressure inlet/outlet velocity documentation](https://doc.openfoam.com/2312/tools/processing/boundary-conditions/rtm/derived/outlet/pressureInletOutletVelocity/)
   likewise distinguishes outward flow from reversed inflow; it is a reference
   for boundary semantics, not evidence for this implementation.
3. **Physical obstacle forces:** integrate resolved pressure and viscous traction
   over the body, compare against independent control-volume momentum, and run
   grid/domain/timestep refinement. Select a reference matching dimension,
   Reynolds number and confinement; do not apply free-space sphere drag to a
   confined 2D object. Only then expose a qualified drag coefficient.

The new integrator is app-owned numerical policy; shared scene/session services
remain reused. No shared API/version change is introduced. Existing production
transport remains upwind; verification-only limited transport is not promoted.

The [stationary obstacle projection](cfd_mac2d_obstacle.md) now implements
matched solid-face pressure topology and measured pressure reaction. Leakage,
known-pressure recovery and projection momentum tests pass. Full obstacle
time stepping remains explicitly disabled pending viscous/advective wall
momentum; agent obstacle authoring and open outlets remain unsupported.
