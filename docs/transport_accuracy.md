# S3 bounded velocity transport

Local Main Edit implementation, 2026-09-27. This follows the measured damping in
[solver_accuracy_audit.md](solver_accuracy_audit.md). It improves the qualification
solver; it does not qualify physical pressure, outlet conditions, or drag.

## Implementation and scope

Qualification mode now selects `bounded_maccormack_visible_donors_v1` in
`src/app/sim_runtime_3d_transport.c`. The legacy visual lane retains its previous
velocity transport. This is an incremental rollout through the existing local
agent session contract, with no new release or Desktop package in this slice.

For each velocity component, frozen previous velocity traces a forward advection
and a reverse advection. With predictor p = A(q) and reverse result r = A_reverse(p),
the candidate is p + (q - r)/2. The result is clamped to the original departure
stencil's minimum and maximum. This avoids new component extrema during transport;
subsequent forces and pressure projection have their own behavior. The method is
related to the bounded MacCormack methods discussed in
[Zhang et al., 2015](https://www.cs.ubc.ca/~rbridson/docs/zhang-siggraph2015-ivocksmoke.pdf).
No general second-order accuracy claim is made for this limited, wall-aware,
explicit-backtrace implementation.

The correction is disabled when a departure/reverse trace leaves the domain,
uses an irregular stencil, or samples a predictor that used a fallback. Exterior
coordinates are clamped for the first-order fallback. Solid donors are excluded
and remaining convex weights are renormalized. Voxel traversal tests the complete
segment, including conservative checks at tied corner crossings. A blocked trace
retains its arrival-cell value; it does not sample through the wall. Each donor
must also be visible from the arrival cell. This replaces the qualification
velocity sampler's old averaging of arbitrary fluid neighbors around a solid.
It is not a cut-cell or moving-wall boundary formulation.

Pressure work arrays are reused before projection, component by component. There
is no new per-cell persistent storage or per-step allocation. Density/dye
advection, SI diffusion, projection, and safety limits retain their existing
contracts. This is velocity advection, not a conservative momentum discretization.

## Agent inspection

Session snapshot `health` includes:

- `velocity_transport`: exact scheme identifier.
- `transport_scope`: explicitly excludes conservative momentum claims.
- `transport_corrected_components`: component samples receiving a correction.
- `transport_limited_components`: subset whose candidate exceeded donor bounds.
- `transport_fallback_components`: fluid component samples using first-order or
  blocked-trace fallback.

Counters aggregate solved regions for the latest step, reset each step, and
exclude solid cells. Corrected plus fallback equals three times the number of
fluid cells processed; limited is a subset of corrected. Legacy transport reports
zero correction counters. These are transport diagnostics, not physical-accuracy
scores. Worker hashes continue to bind all numerical studies to an exact binary.

## Independent acceptance tests

The isolated operator test transports a transverse cosine at U = 2 m/s for
0.2 seconds. It uses enough transverse padding to exclude clamped-edge influence
and tests amplitude and phase against the continuum solution. Separating the
operator from pressure avoids conflating transport error with a tiny test domain's
projection-boundary effects. Full-solver integration is covered separately.

Acceptance budgets for this fixture are less than five percentage points absolute
amplitude error and less than one quarter cell of phase displacement. These are
project screening budgets, not a general CFD accuracy standard. The previous
method's analytically derived amplification is a comparison, not the new target.

| h (m) | dt (s) | Previous first-order retention | New retention | New phase error (rad) |
|---|---|---|---|---|
| .0625 | .020 | .836498 | .978566 | .037063 |
| .0625 | .010 | .713989 | .966068 | .059698 |
| .0625 | .005 | .661353 | .964246 | .066566 |
| .03125 | .010 | .914904 | .995234 | .009494 |

Amplitude loss drops substantially. Phase error is larger than the old method in
these fixtures; it remains within the quarter-cell budget and decreases on the
finer grid. Smaller timesteps still do not eliminate spatial error, and the
coarse-grid method still has approximately 2–4% inviscid amplitude loss.

With nu = .01 m^2/s, h = .0625 m and dt = .01 s, exact viscous retention is .924080;
measured retention is .895448 and phase error is .059245 rad, within the same
budgets. Existing SI diffusion tests independently verify viscosity response and
grid refinement. Physical viscosity is not retuned to hide numerical damping.

Additional direct tests cover constant-component preservation, sharp-step bounds,
all-six-direction one-cell-wall separation at a three-cell departure, diagonal
solid-corner blocking, and counter accounting. Existing inlet, projection,
conservation, and legacy stable regressions remain required.

## Numerical acceptance boundary

Matched seven-second Wind measurements and final test logs are recorded in
`build/s3-transport-acceptance/`. An empty-channel integration passes the steady
screen. The sphere wake remains unsettled in the first integrated run despite
converged pressure and no clamps. Changed wake shape alone is not evidence of
physical accuracy. Do not advance to physical pressure/viscous force acceptance
based solely on the improved transport test.

The next gates are grid/timestep studies with this exact method, a consistent
physical outlet and momentum/face-flux contract, and low-Re reference baselines.
For genuinely unsteady wakes, statistical stationarity needs a separate criterion
rather than loosening the steady screen until it passes.

## Ownership

Reuse scan: existing core_math, core_space, core_scene, and core_sim contracts
remain in place. Shared libraries do not own this application's fluid
interpolation, solid-voxel policy, or solver scratch buffers. New numerical
abstraction is `reuse-deferred`, app-local; no shared API, version, or adoption
change is needed. Runtime scheduling and scene geometry continue through the
existing shared contracts.

## Final matched Wind readback

Both cases use the same final worker, sphere D = .3 m, U = 2 m/s, air constants,
32 x 16 x 16 cells, a 2 m tunnel, and seven seconds of evolution.

| dt (s) | Near central mean (m/s) | Far central mean (m/s) | Steady status |
|---|---|---|---|
| 0.01 | 0.154097 | 0.668973 | not_steady_or_numerically_unresolved |
| 0.02 | -0.042898 | 0.950011 | not_steady_or_numerically_unresolved |

Both runs have converged recorded pressure solves and zero recorded velocity
clamps. Near/far trailing-window means differ by 9.85% / 14.05% of inlet speed.
The timestep screen rejects the pair: fewer than three levels, excessive
sensitivity, and unsettled cases. These are transient comparisons, not a temporal
convergence estimate. Removing artificial damping does not automatically make
this coarse high-Re wake more predictable. This result supports moving to
controlled low-Re and boundary/momentum verification before accepting drag.
