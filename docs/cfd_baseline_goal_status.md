# Active CFD baseline goal

Current continuation: [component-force and obstacle-energy assessment](cfd_component_energy.md)
and [automated run acceptance](cfd_run_acceptance.md) supersede earlier statements
that masked energy is unsupported or that separate force errors are only 3–5%.
The component 2% gate remains open; 3D work has not started.

The requested end state is unchanged: (1) close one obstacle-force accuracy case,
(2) implement and qualify real inlet/outlet behavior, and (3) expose a small CFD
lab through the existing agent scene/session interface. This document records
progress and outstanding proof; it is not a completion certificate.

## Obstacle force: fixed periodic-case consistency gate now passes

The default masked MAC solver now weights partially covered corner dual faces
by their wall coverage. The old tangential ghost treated a half-covered face as
a full wall, adding excess corner damping. The corrected flux combines half-cell
distance over the wall portion and full-cell distance to the constrained normal
face over the remaining portion. The matching discrete viscous reaction uses the
same geometry. Full straight walls and normal impermeability are unchanged.

Independent flux quadrature tests cover all rectangle corners, straight faces,
both velocity components and two grid spacings. The legacy stencil fails this
regression; the correction passes, including address/undefined-behavior checks.
The existing planar shear and pressure reconstruction references still pass.

With the established force reconstruction and unchanged 2% gate:

| Grid | Surface traction N | Independent CV force N | Relative mismatch |
|---|---:|---:|---:|
| 16² | .01831872 | .01890857 | 3.119% |
| 32² | .01867101 | .01908791 | 2.184% |
| 64² | .01884295 | .01915915 | **1.650%** |

`make qualify-cfd-mac2d-force` now reports `accuracy_passed: true` while retaining
`physical_drag_qualified: false`. The higher-order reconstruction remains a
diagnostic candidate (0.320% fine-grid mismatch, nonmonotonic over the three
grids), not the implementation chosen to pass the gate. The existing surface
formula passes through a correction to the actual velocity operator.

Masked momentum tests retain zero leakage, residual below 9.63e-14 N, divergence
below 3.39e-12, and unforced energy decay. Boundary-pressure/baffle tests retain
the exact .075 N reconstructed pressure reference and explicitly quantified
unresolved drive volume; the raw discrete reaction is not relabeled continuum
traction. Core unmasked MAC, obstacle and seven agent CFD regression tests pass.

This closes the bounded fixed-periodic-case consistency gate, not external drag
validation, a formal steady-window claim, or open-boundary obstacle qualification.
Evidence: `build/s3-corner-correction/` and
`build/s3-cfd-force-control-volume/report.json`. Reproduce corner checks with
`make test-cfd-mac2d-corner` and the force gate with
`make qualify-cfd-mac2d-force`.

### Historical diagnosis before the corner correction

`cfd_mac2d_force_check` integrates pressure, symmetric viscous stress, advective
momentum and geometric forcing on a rectangular control surface separated from
the body. Fluid momentum inside that surface is independently integrated from
cell-centred velocities. Body traction uses wall-reconstructed pressure and a
quadratic no-slip tangential derivative `(9*u_near-u_next)/(3*h)`.
A supplied planar-wall field independently verifies the shear magnitude/sign.

For a stationary no-slip incompressible straight wall, the tangential wall
velocity is constant, so continuity requires zero normal strain. The code keeps
its reconstructed violation in `reconstructed_normal_viscous_x_n` instead of
counting it as physical wall traction. Including this nonphysical contribution
had misleadingly improved apparent agreement. Corner singularities are not
resolved by assuming a nonzero normal strain on the open straight faces.

Periodic block, domain 4x2x.5 m, block [1.5,2.5] x [.75,1.25] m, rho=1,
mu=.1, G=.01, T=10 s:

| Grid | Surface pressure+shear N | Independent CV force N | Relative mismatch |
|---|---:|---:|---:|
| 16² | .0173733 | .0191630 | 9.34% |
| 32² | .0180340 | .0191949 | 6.05% |
| 64² | .0184163 | .0192058 | 4.11% |

`make qualify-cfd-mac2d-force` records the comparison and fails while the finest
mismatch exceeds the 2% engineering consistency gate. That threshold does not
constitute experimental drag validation. A positive execution result from the
lower-level fixture alone is not force qualification. The corner correction above addresses the observed geometric error without
adding the normal-strain artifact back to obtain a pass. A placement and endpoint-time screen is recorded below. Formal steady-window
screening and a matched reference with declared scope remain required.

A higher-order reconstruction candidate adds quadratic wall-pressure extrapolation
and a cubic no-slip shear derivative. Supplied polynomial pressure and planar
shear fields pass exact-value checks. On the same computed block flows, mismatch
falls to 6.52%, 4.27%, and 3.10% at 16/32/64. This changes measurement only, not
the flow operator. It remains a diagnostic candidate: the 2% gate is unchanged,
and default live forces retain the established reconstruction. Evidence:
`build/s3-cfd-followup/report.json` and `force-candidate.log`.

### Control-surface placement and endpoint-time screen

The force fixture now measures four fixed physical surrounding rectangles,
independently varying X and Y offsets, at T=5 and T=10 s on all three grids.
All 24 observations retain the independently integrated momentum derivative.
At T=10, surrounding-force spread is 0.0531%, 0.0269%, and 0.0134% for
16/32/64 cells. At 64 cells the candidate traction mismatch stays between
3.087% and 3.100% across placements. The maximum momentum-rate contribution
at T=10 is 0.00424% of the inferred force on the finest grid. Its force changes
0.547% between T=5 and T=10; this is an endpoint sensitivity screen, not a
formal steady-window or monotonic convergence proof.

These pre-correction measurements make surrounding-surface placement and late transient
momentum too small to account for the approximately 3.1% discrepancy in this
case. They motivated the near-body corner correction recorded above. No acceptance
threshold was changed during that investigation.

Reproduce with `make test-cfd-mac2d-force-accuracy`, save the output, then run
`python3 scripts/assess_cfd_force_placement.py LOG --output REPORT.json`.
The assessment requires all 24 unique observations and gates placement spread
below 0.5% and endpoint force change below 1%, separately from force accuracy.
Evidence: `build/s3-force-placement/run.log` and `report.json`.

## Inlet/outlet: actual numerical prototype implemented

`CfdOpen2D` uses `(Nx+1)*Ny` independent X-face velocities, cell-centred pressure,
and `Nx*(Ny+1)` Y-face velocities. X is NOT periodic. The left boundary prescribes
a parabolic normal velocity and zero tangential velocity. The right boundary
sets physical pressure to zero. Its half-cell Dirichlet contribution enters the
pressure operator and normal face correction; the inlet has a pressure-correction
Neumann condition. Y walls are stationary no-slip. Projection has no constant
nullspace under the fixed outlet pressure and checks a recomputed true residual.

The predictor uses MC-limited momentum reconstruction and explicit viscosity.
The outlet normal predictor now advances momentum on its half dual volume; its
final normal velocity is solved through pressure correction, not clamped. Tangential outlet velocity
uses zero gradient for outward flow and a zero-velocity reservoir on reversed
inflow. Signed reversed flow remains measurable. The core rejects over-CFL
steps instead of silently changing dt. The prototype now supports one stationary interior rectangle, using matched
masked divergence, pressure operator and face correction plus wall-coverage
corner viscosity. No release or agent model has adopted it.

`make test-cfd-open2d` tests channel preservation/recovery from an initialized
parabolic profile, pressure drop and mass balance at 8/16/32 grids for L=2 and
L=4 m, H=1, width=.5, rho=1, mu=.1, continuous mean inlet speed .05 m/s.
The prescribed cell-centre parabola's discrete flux is retained and disclosed;
it is not renormalized to hide quadrature error.

After the quadratic wall-flux correction, at 32² inlet pressure is
.119999999994 Pa versus .12 Pa for L=2 and .239999999966 Pa versus .24 Pa for
L=4. Velocity error is below 3.76e-13 m/s and mass mismatch below 1.12e-13 m³/s. The known analytic reference is fully developed
laminar Poiseuille flow; this is a low-Re empty-channel reference, not an obstacle
or wake benchmark. These fixtures are not startup-from-rest verification.

A seeded outlet-touching streamfunction pulse creates .04820 m³/s initial reverse
flow. After projection the measured reverse-flow maximum is .04739 m³/s; it decays
to zero over .5 s while mass balance and divergence checks pass. This is a bounded
backflow robustness screen, not an independently validated backflow solution.

`make test-cfd-open2d-exit` now passes a bounded streamfunction-pulse exit and
matched-spacing domain-extension test (L=2 versus L=4, two resolutions, Re=20,
T=12 s). After the wall correction, peak upstream domain differences are 0.837% and
0.814% of the initial disturbance amplitude; late differences are below 0.457%.
Final short-domain disturbance energy fractions are 7.77e-8 and 5.57e-8. Baseline subtraction
separates the pulse from channel adjustment. This is combined advection/diffusion
exit evidence, not a general reflection-free boundary claim.

The new obstacle coupling and transient force-consistency test is documented in
[cfd_open2d.md](cfd_open2d.md). At 16/32/64 grids, surface/CV discrepancy is
2.764%, 1.955%, and 1.479%; the finest grid passes the unchanged 2% gate.
Zero leakage, mass balance and symmetry pass. Initial velocities are the
channel parabola clipped at the solid; T=1.001 s is not claimed steady.

An independent channel momentum/work/dissipation audit now passes (see below).
Outstanding: nonuniform outlet and masked momentum/energy audits, quantitative startup/transient
reference verification, and a confined low-Re obstacle reference. Do not claim the full
outlet qualification complete. Evidence: `build/s3-cfd-followup/outlet-exit.log`.

## Channel work/dissipation audit and wall correction

The audit exposed a second wall error: reflected tangential velocity preserves
linear no-slip fields but offsets a quadratic Poiseuille profile. Reconstructed
wall shear then had first-order error, giving a 2.05% momentum-budget mismatch
at 32². Open-solver outer-wall diffusion now uses the quadratic-consistent flux
`(9*u0-u1)/(3*dy)`, equivalent to ghost `-2*u0+u1/3`. Advective wall evaluation
is unchanged. The known parabola and pressure drop now persist to numerical
precision. This correction does not change the periodic solver's outer walls.

`cfd_open2d_budget` independently integrates momentum, kinetic energy, pressure
and viscous boundary work, advective momentum/energy flux, wall shear and strain
dissipation. It is observational and rejects solid masks until their energy
quadrature is implemented. Callers retain finite-difference time derivatives.

`make test-cfd-open2d-budget` verifies exact supplied pressure force and wall
shear and the analytic dissipation reference. At 32², relative momentum residual
is 4.72e-11. Relative energy residual is .146484%, exactly the quadrature law
1.5/Ny²: midpoint inlet flow is high by .5/Ny² and dissipation is low by 1/Ny².
Both components are disclosed rather than normalized away. The residual drops
fourfold per doubled grid. Sanitizer, channel/backflow, and disturbance-exit
gates pass. The open-obstacle force gate remains passing at 1.478% mismatch.

Evidence: `build/s3-open-energy/`. This is empty-channel calibration of the
budget, not full wake/outlet energy acceptance or a matched obstacle reference.

## Historical donor-energy diagnosis before transport/outlet correction

The disturbance-exit fixture now integrates pressure/viscous work, outward
kinetic-energy transport, viscous dissipation and momentum impulse for each
perturbed and unperturbed run. Budgets start after the first projection because
initial pressure is not initialized. Subtracting the matched unperturbed budget
prevents steady channel power from hiding pulse-specific errors. The remaining
energy is normalized by initial excess kinetic energy, not total channel energy.

| Transport | Short-grid Nx/Ny | dt | Short L=2 residual | Long L=4 residual |
|---|---|---|---:|---:|
| Previous donor | 24/16 | .002 | +167.18% | +204.77% |
| Previous donor | 32/24 | .002 | +117.46% | +144.90% |
| Diagnostic centered | 24/16 | .002 | -3.37% | +0.91% |
| Diagnostic centered | 32/24 | .002 | -3.57% | +0.41% |
| Diagnostic centered | 32/24 | .001 | -3.88% | +0.47% |

Residual is integrated net physical power minus kinetic-energy change. Positive
values are energy removal unexplained by measured physical dissipation/flux.
The inlet maintains flow and can supply additional energy, so residual above
100% of the small initial pulse does not imply removal of more than the total
flow energy. The isolated switch removes only donor diffusion: it strongly
implicates transport dissipation as the dominant error here. Centered transport
is compile-time diagnostic only and is not promoted as a robust runtime method.

Both domain-exit gates still pass. The residual short-domain discrepancy does
not decrease when dt is halved and does not improve over these two spatial
resolutions, unlike the longer-domain discrepancy. It needs its own outlet/
quadrature investigation after transport is improved. These measurements do
not constitute energy acceptance for either transport scheme.

This diagnosis led to the default correction below. Reproduce historical donor
rows with `make test-cfd-open2d-exit-donor`, centered rows including half dt with
`make test-cfd-open2d-exit-centered`. Evidence:
`build/s3-nonuniform-energy/report.json` and accompanying logs.

## Default open transport and outlet correction: bounded pulse gate passes

The open solver now uses conservative shared-face MC-limited momentum
reconstruction. Its reconstructed states remain between neighboring values;
standalone square advection is bounded and conservative in both directions at
Courant .4. Smooth-advection L1 error falls by factors 3.99 and 4.07 over doubled
grids with dt proportional to dx². This does not assert a scalar maximum principle
for the complete projected Navier-Stokes update.

Limited transport alone reduced energy error but narrowly failed the coarse
1% domain-sensitivity gate (1.024%). That limit was retained. The outlet now
advances its own half-dual-volume momentum using signed advective boundary flux,
zero outward normal viscous flux, and the interior viscous flux. Its pressure
correction retains the same matched half-cell fixed-pressure condition. The
copied-neighbor predictor is retained only as a diagnostic compile option.

| Short-grid Nx/Ny | dt s | Short energy residual | Long energy residual |
|---|---:|---:|---:|
| 24/16 | .002 | 4.353% | 4.816% |
| 32/24 | .002 | 1.577% | 1.784% |
| 32/24 | .001 | 1.637% | 1.847% |

Residual uses the same integrated, baseline-subtracted physical budget and
initial excess energy as the historical diagnosis. `test-cfd-open2d-exit` now
enforces a 2% engineering consistency gate on both fine-grid domains at both
timesteps. Coarse rows demonstrate spatial sensitivity, not 2% acceptance.
Existing exit and 1% domain-sensitivity gates pass at both spatial resolutions;
peak upstream domain difference is approximately .25% of initial pulse amplitude.
This is acceptance for this bounded Re20 pulse, not a universal outlet guarantee.

The promoted default passes exact channel pressure/velocity preservation,
calibrated empty-channel energy, backflow, and open-obstacle tests. Fine obstacle
force mismatch is 1.474%; zero leakage and mass/symmetry checks remain passing.
Address/undefined-behavior checks cover the widened boundary stencils and body
coupling. Evidence and current source hashes: `build/s3-limited-outlet/`.

The independent low-Re reference result is recorded below. Open-model agent
integration remains required; any masked-energy claims need their own quadrature proof.
The existing periodic agent solver is unchanged by this open-solver correction.

## Independent confined low-Re total-drag reference: bounded screen passes

A separate P2/P1 Taylor-Hood finite-element Stokes calculation now provides a
reference for the same 1 x .5 m rectangle in the 4 x 2 m channel, width .5 m,
mu=.1 Pa s, rho=1 kg/m³ and mean inlet .002 m/s (body-height Re=.01).
It uses scikit-fem 12.0.2 with pinned verification-only dependencies. The exact
empty-channel velocity and pressure errors are below 8.3e-18 m/s and 4.8e-17 Pa.
This is independent numerical verification, not experimental validation.

Reference variational-reaction drag over transverse FEM grids 16/32/64/128 is
.005910113, .005947624, .005965058 and .005973307 N. Last refinement changes total
drag .138%; direct surface traction agrees with the finest reaction within .011%.
The reference outlet uses zero vector-Laplacian traction; the MAC solver uses
its pressure-outlet momentum formulation. Formal equivalence is not asserted.
Doubling the downstream domain changes reference and MAC control-volume drag
by about .0017%, providing a bounded outlet-sensitivity screen for this case.

MAC runs satisfy two consecutive one-second velocity-change windows below
1e-6 of inlet speed, with minimum time 5 s; all three grids settle at 8 s.
The established lower-order surface reconstruction had 2.56% fine-grid error
against the reference despite passing its self-consistency gate. Open-force
observation now uses quadratic pressure and cubic no-slip shear reconstruction,
calibrated on supplied polynomial fields. It requires all third fluid samples
and does not silently fall back. The periodic estimator is unchanged.

At 64², corrected surface drag is .005890113 N (1.393% reference error), while
the surrounding momentum estimate is .005906724 N (1.115%). Halving inlet speed
changes normalized CV force by about .00018%, much smaller than discretization
error. Surface reference error decreases across the three MAC grids. Exact
reconstruction, transient obstacle, steady-reference and sanitizer checks pass.

Separate pressure and viscous contributions are NOT independently qualified:
finest differences from the current FEM component estimates are approximately
4.85% and 3.45%, and the reference components converge more slowly than total
reaction. Do not describe the 2% total-drag screen as a 2% component guarantee.
`build/s3-independent-reference/comparison.json` records all ten passing checks,
source hashes and the scope boundaries. Reproduction is in
[cfd_independent_reference.md](cfd_independent_reference.md).

Open-model agent integration is now verified below; masked-energy diagnostics
remain unsupported.

## Agent lab: usable periodic obstacle slice implemented

The revision-bound `cfd_obstacle_2d` preset authors a stationary grid-aligned
rectangle with explicit SI fluid parameters. Live inspection and history expose
discrete pressure/viscous reactions, conservation residuals, reconstructed body
traction and an independent surrounding-control-volume comparison. Solid masks
are sampled and included in hashed terminal field artifacts. `run_compare`
compares matched completed runs under spatial or timestep refinement, rejecting
incompatible scenes, times, fluids and worker binaries. It labels differences
as sensitivity, never inferring reference accuracy or steady state.

The open-boundary model now also participates in the shared agent interface.
See [CFD agent lab](cfd_agent_lab.md) for its separate case-specific qualification.
Durable proof: `build/s3-agent-cfd-lab/` contains three completed runs, force
histories, hashed result manifests, and spatial/temporal comparison reports.

The bounded baseline deliverables are implemented and verified: fixed-body
force consistency and independent total-drag screening, physical inlet/outlet
checks, and the agent laboratory. This does not close general CFD qualification.
No commit or desktop refresh is performed.
The new numerical modules remain app-owned; shared scene/session reuse continues,
with no shared API/version changes. Work remains in the existing Main Edit tree.

## Open agent integration acceptance

The channel and stationary-obstacle open presets pass the real worker lifecycle,
initial field diagnostics, physical-pressure/mask samples, force history, hashed
terminal exports and matched spatial/time refinement tests. All 19 tests across
open, periodic laboratory, MAC, reduced channel and general session suites pass.
Durable demonstration: `build/s3-open-agent-lab/` (three completed runs at T=.2 s).
These short runs demonstrate controls and diagnostics, not steady-state accuracy.

Remaining useful engineering gates are: independently qualify force components
and masked energy; automate per-run steady-window and refinement acceptance;
then carry the verified flux/projection approach into a bounded 3D channel before
curved STL bodies, moving walls, adaptive resolution or turbulence. The present
CFD baseline is exclusively 2D, stationary, laminar and bounded to 64 cells per
axis. Existing 3D Wind remains a separate approximate path.
