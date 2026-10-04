# Known-answer nonuniform transient and transport isolation

This S3 checkpoint establishes a manufactured solution, isolates a dominant
transport error in that smooth fixture, and separately measures pressure timing
and wall-gradient behavior. It does not qualify obstacles, outlets or forces.
The agent-selected scheme remains first-order donor-cell upwind. Centered and
minmod-limited reconstructions are verification-build options only.

## Known solution and independent forcing check

Domain: L=2 m, H=1 m, width=.5 m; rho=1 kg/m3, mu=.01 Pa s.
Periodic X and stationary no-slip Y walls; no prescribed mean pressure gradient.
For k=pi, define

```
psi = A exp(-t) sin(k x) sin²(k y)
u = A exp(-t) k sin(k x) sin(2 k y)
v = -A exp(-t) k cos(k x) sin²(k y)
p = P exp(-t) cos(k x) cos(2 k y)
f = du_vector/dt + (u_vector dot grad)u_vector + grad(p)/rho - nu laplacian(u_vector)
```

The decay constant is 1/s; A=.01 m²/s and P=.01 Pa. The continuous field is
divergence-free and satisfies both wall velocity conditions. Analytic spatial
and time derivatives generate acceleration f in m/s². A separate central finite
difference calculation of derivatives of the exact field checks the analytic
forcing at 49 off-grid positions, with an absolute discrepancy below 1e-7 m/s².
The runtime never reads the expected solution: it samples only the force callback.
Initial face velocities are sampled from the exact field and projected once;
that initialization error is included in final velocity errors.

`cfd_mac2d_step_forced` evaluates the optional callback at face locations and
substep start. Its integrated streamwise force is included in the momentum
balance. The ordinary step calls this with NULL, preserving agent behavior.
Nonfinite force is rejected. This hook is not scene-authorable or a general
arbitrary-force stability contract; the benchmark checks that each prescribed
tick uses one substep and that fields, divergence and momentum remain bounded.

## Experiment controls

Integrate to .1 s at grids 8/16/32/64 with dt=.00025 s, plus timestep studies
at 32 and 64. Three numerical builds change only convective face reconstruction:

- Upwind: current production default.
- Centered: removes donor-cell dissipation as a diagnostic; not bounded.
- Limited: minmod slopes reconstruct left/right face values before upwinding.
  This is a conservative candidate, not a globally certified bounded scheme.

Each build runs the coupled solution, a zero-velocity pressure-only solution,
and a zero-velocity case with `p=P exp(-t) cos(k x) sin(k y)` that has nonzero
normal pressure gradient at the walls. The latter requires physical forcing to
balance pressure; normal wall velocity remains zero. Wall-band pressure errors
use the bottom/top eighth of the domain, not a resolution-dependent single row.
Pressure is compared at both final time and the final substep start.

## Results

Velocity L2 error against the exact final solution, in m/s:

| Grid | Upwind | Centered diagnostic | Limited candidate |
|---|---:|---:|---:|
| 8² | 2.4852e-5 | 3.7565e-5 | 2.3960e-5 |
| 16² | 2.3449e-5 | 9.338e-6 | 6.878e-6 |
| 32² | 1.4437e-5 | 2.160e-6 | 1.734e-6 |
| 64² | 7.917e-6 | 3.63e-7 | 3.56e-7 |

Changing only transport reconstruction reduces fine-grid velocity error about
22 times. The limited candidate decreases error across all four grids. This
identifies donor-cell dissipation as the dominant removable error in this
particular coupled fixture; it is not proof about every flow. Near-second-order
trends in the candidate are not a formal global order certificate, because the
first-order time error can cancel spatial error at the finest resolution.

At 64², coupled pressure error changes from 4.269e-6 Pa (upwind) to
3.046e-6 Pa (limited), a smaller improvement. This motivates separating pressure
error from velocity transport rather than interpreting all pressure error as
advection error.

For the pressure-only control at 64²/dt=.00025, final-time pressure error is
2.949e-6 Pa; comparing at substep start gives 1.818e-6 Pa. The corresponding
32² start-time error is 7.278e-6 Pa, giving a spatial ratio of about four.
Halving the 64² timestep reduces final-time error to 2.383e-6 Pa while the
start-time error stays approximately 1.818e-6 Pa. This demonstrates a temporal
staggering contribution independently of transport. It does not justify simply
relabeling all coupled pressures as exact at the earlier time.

The nonzero wall-gradient control reduces spurious velocity from 1.0323e-5
m/s at 8² to 1.51e-7 m/s at 64², with approximately second-order refinement.
Its 64² pressure wall-band error is 1.630e-6 Pa versus 3.137e-6 Pa in the
interior at final time. These controls do not show a dominant nonconvergent wall
error; arbitrary moving walls and obstacle boundaries remain untested.

## Reproduction and gate status

`make test-cfd-mac2d-manufactured` builds all three variants and runs 72 cases.
The JSON report and binary hashes are in `build/s3-mac2d-manufactured/report.json`.
The gate checks decreasing limited velocity error, at least fivefold fine-grid
improvement over upwind, near-second-order time-aligned pressure refinement,
transport-neutral pressure-only results, and wall-gradient refinement. These
are regression guards for this measured checkpoint, not physical acceptance
thresholds or experimental validation. Every run checks divergence <1e-8 s^-1,
streamwise momentum residual <1e-9 N and one internal substep per tick.
Existing MAC core and both channel agent suites pass. The limited 32² coupled
case passes address/undefined-behavior sanitizers.

Next gates, in order:

1. Exercise limited transport on sharp gradients, reversals and longer unforced
   decay; check extrema, energy behavior and conservation before runtime adoption.
2. Expand known-answer pressure timing/splitting tests in coupled flow. Change
   the pressure scheme separately only if measurements justify it.
3. Introduce stationary obstacle geometry with verified wall momentum exchange,
   then physical inlet/outlet balances and pressure/viscous force integration.
   Compare controlled low-Re cases and domain/grid sensitivity before claiming
   drag accuracy. No obstacle/outlet/force acceptance is granted by this campaign.

Shared ownership is unchanged: app-owned numerical verification and solver hook;
existing scene/session interfaces reused without new shared API/version changes.
