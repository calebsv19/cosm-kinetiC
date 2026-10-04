# Open-boundary 2D CFD prototype

Current continuation: [component-force and obstacle-energy assessment](cfd_component_energy.md)
and [automated run acceptance](cfd_run_acceptance.md) supersede earlier statements
that masked energy is unsupported or that separate force errors are only 3–5%.
The component 2% gate remains open; 3D work has not started.

This app-owned core is available through the open channel/obstacle agent presets,
separate from the periodic agent model. It uses a
prescribed parabolic inlet, a fixed-pressure outlet with a half-cell Dirichlet
pressure contribution, and stationary no-slip Y walls. The outlet normal velocity
is pressure-corrected, not clamped. Reversed outlet flow is measured; tangential
backflow uses the existing zero-velocity reservoir rule. See
[cfd_baseline_goal_status.md](cfd_baseline_goal_status.md) for channel, backflow
and disturbance-exit evidence and remaining acceptance gates.

## Stationary obstacle

`cfd_open2d_set_obstacle` installs one cell-aligned interior rectangle before the
first step, with at least two fluid cells to every domain edge. It rejects repeated
installation and invalid bounds. The mask is solver-owned and freed on destruction.
Blocked normal face velocities are zero. Pressure gradients and the pressure
operator omit exactly those blocked connections; solid pressure rows are isolated
and remain zero. Fluid cells still connect to the physical pressure outlet.

Viscous tangential ghosts account for half-covered corner faces using the same
geometric rule proved for the periodic MAC solver. Limited shared-face momentum transport has
zero flux into blocked dual faces. With no obstacle, the original channel path
and its numerical results are retained.

Installing the rectangle clips the initialized channel parabola at blocked
faces. The first projection resolves that initial divergence. This is a declared
transient initial condition, not an established steady solution or a validated
physical inlet-startup experiment. The current core has no moving bodies or
arbitrary mesh import.

## Tests and force scope

`make test-cfd-open2d-obstacle` runs 16/32/64 grids on a 4 x 2 x .5 m domain,
rectangle [1.5,2.5] x [.75,1.25] m, density 1 kg/m³, dynamic viscosity .1 Pa s,
and mean inlet speed .02 m/s (height-based Re=.4). It advances 1 s with dt=.001 s,
then takes a second force observation at T=1.001 s to retain the momentum rate.
It checks impermeability, mass conservation, mirror symmetry, flow diversion,
and increased inlet pressure relative to an empty-channel run.

Independent physical-space quadrature measures pressure/shear on the body and
pressure, viscous traction, advective flux and momentum change on an interior
surrounding rectangle. A test-only storage adapter reuses that quadrature; it
neither advances the periodic solver nor wraps across an open domain boundary.
No body-drive or fictitious affine pressure is inserted.

| Grid | Surface force N | Surrounding momentum force N | Mismatch |
|---|---:|---:|---:|
| 16² | .05365610 | .05518124 | 2.764% |
| 32² | .05694402 | .05807919 | 1.955% |
| 64² | .05829419 | .05916948 | 1.479% |

The finest case enforces the same 2% force-consistency threshold as the periodic
case. Across all ticks, leakage is zero, mass mismatch is below 1.70e-12 m³/s,
and divergence is below 2.42e-11 s⁻¹. A 16² address/undefined-behavior sanitizer
run passes. Existing channel-pressure and backflow fixtures also pass unchanged.
Evidence is in `build/s3-open-obstacle/`.

This is solver coupling and transient consistency evidence. It is not a matched
external drag reference, an outlet energy-budget proof or an open-boundary agent
preset. Those remain required before this becomes the requested qualified
wind-tunnel baseline.

## Momentum and energy diagnostics

`cfd_open2d_budget` reports momentum and kinetic energy inventories, signed
pressure and viscous work, outward momentum and kinetic-energy fluxes, outer
wall force and viscous dissipation. Two observations are needed for time rates.
It currently rejects solid masks; values must not be presented as obstacle
energy diagnostics. The API uses physical-space quadrature rather than an
algebraic restatement of the solver update.

`make test-cfd-open2d-budget` checks the supplied Poiseuille solution against
analytic force and power. That audit exposed and corrected reflected-ghost
wall diffusion: outer-wall viscosity now uses a quadratic-consistent no-slip
flux, while advective wall evaluation is unchanged. The channel parabola and
pressure drop now remain accurate to numerical precision. Energy integration
retains a known 1.5/Ny² relative midpoint-quadrature residual (.146% at Ny=32).

The earlier obstacle table above records the initial coupling campaign. After
this outer-wall correction the finest force mismatch is 1.478%, still below
2%; channel, backflow and disturbance-exit gates also pass. Current evidence:
`build/s3-open-energy/`. Nonuniform wake/outlet and masked energy audits remain
outstanding, along with the external confined low-Re obstacle reference.

## Historical nonuniform energy limitation

The exit fixture now integrates baseline-subtracted pulse energy and momentum
budgets. The former donor scheme had large pulse-specific energy residuals:
117% and 145% of initial excess energy at the finer short/long domains. A
compile-time centered-transport diagnostic reduces these to -3.57% and +.41%,
strongly isolating donor dissipation. Halving its dt does not remove the remaining
short-domain discrepancy. Neither method is declared energy-qualified.
See [the full measured comparison](cfd_baseline_goal_status.md) and
`build/s3-nonuniform-energy/report.json`. `test-cfd-open2d-exit-centered` is an
investigation target, not a runtime setting or a release qualification gate.

## Current transport and outlet acceptance

The default now uses MC-limited reconstruction and a half-dual-volume outlet
momentum predictor. The outlet velocity is still corrected through the matched
fixed-pressure projection; it is no longer copied from its neighbor. A bounded
Re20 pulse passes the original exit/domain-length gates and a 2% fine-grid energy
consistency gate at dt=.002 and .001 s (short/long residuals 1.577/1.784% and
1.637/1.847%). Coarser results are 4.353/4.816%, explicitly not 2% acceptance.

`make test-cfd-momentum-flux` verifies bounded reconstructed states, conservative
square-wave transport in both directions, and smooth refinement. The complete
flow is also checked with channel, backflow and obstacle tests; face limiting
alone does not prove global velocity boundedness after pressure projection.
`make test-cfd-open2d-exit` runs the current qualified pulse slice. Historical
comparisons are reproducible with `test-cfd-open2d-exit-donor` and
`test-cfd-open2d-exit-centered`, both using the diagnostic copied outlet.

`build/s3-limited-outlet/report.json` binds current results to source hashes.
External obstacle drag and masked energy remain unqualified; the open solver
is not yet an agent model. See the baseline status for the full goal boundaries.

## Independent total-drag reference

The open force API now uses polynomial-calibrated quadratic pressure and cubic
wall-shear reconstruction, with required third fluid samples. A fixed confined
Re=.01 rectangular-body case passes a 2% total-drag comparison against an
independently refined P2/P1 Stokes reference. Individual components and arbitrary
scenes remain unqualified. See [reference scope and reproduction](cfd_independent_reference.md)
and `build/s3-independent-reference/comparison.json`. Open-model agent integration
remains outstanding.

## Agent integration

The open model now supports the shared session lifecycle, validation, sampling,
force histories, refinement comparison and hashed field export. See
[cfd_agent_lab.md](cfd_agent_lab.md) for the current contract and qualification limits.
