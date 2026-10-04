# C3D-8 symmetric-reference robustness and pressure attribution

2026-09-30, Main Edit. **Implemented the reference measurement/refinement and
pressure-attribution pipeline. Physical reference qualification is still blocked.**
The [experiment contract](cfd_obstacle3d_reference_refinement_goal.md) preserves
all [original gates](cfd_obstacle3d_goal.md). This supersedes the next-step/status
claims in the [previous correction checkpoint](cfd_obstacle3d_correction_checkpoint.md).
No native runtime source, solved field, commit, package or installation changed.

## Delivered behavior

Independent reference traction now records every body plane, pressure, full
symmetric viscous stress, normal/tangential contributions and edge-distance bands.
Facet quadrature orders 2/4/8 agree to better than 1e-11 N on every numerical
reference. A quadratic planar no-slip shear/affine-pressure fixture independently
verifies signs, plane areas, closed loads and edge-band partitioning. Surface
integration arithmetic is not the dominant observed error.

Reference meshes can refine a conforming fluid octant near physical cube edges,
reflect it without post-solve force averaging, allocate subdivisions separately
by axis, and retain physically identical near-body X nodes across duct lengths.
Boundary-facet validation rejects cracks on artificial symmetry planes. Every
solve freezes its exact source/helpers and binds source, output, pressure snapshot
and log hashes. Own-child mesh/RSS/deadline limits retain rejected jobs without
silently retrying or raising admission limits.

The reference pressure preconditioner scales its pressure-mass Schur approximation
by mu+gamma for consistent grad-div controls. MINRES can stop on monitored true
residual <=1e-10; the original <1e-8 acceptance, 3000 iterations and resource caps
remain unchanged. Gamma0 same-mesh force/Pin/energy agrees with the predecessor
to 1.26e-12 relative. The stronger gamma1 control now converges, resolving its
previous numerical iteration failure, but still fails physical force agreement.
Numerical convergence does not turn it into an accepted reference.

Pressure attribution clips each independent P1 tetrahedron against Cartesian
near-body slab boxes and integrates affine pressure exactly by polyhedron moments.
No point sampling, native pressure values or drag calibration enters that
projection. Applying the native pressure trace to those exact reference cell
averages gives a reproducible decomposition into reconstruction and solved-field
force-functional errors. Global affine, piecewise-affine/kink and gauge-shift
analytic controls verify clipping and volume/pressure integration.

Native quadratic pressure averages with cross terms recover independently
integrated body surface force to roundoff on both lengths and all admitted grids.
A square-root pressure-trace control demonstrates that this smooth-polynomial
exactness does not certify limited-regularity fields: its known error decreases
as sqrt(h), rather than becoming exact. This is a mathematical counterexample,
not a Stokes solution or an identified physical corner exponent.

Existing scene/session/MCP, allocator/MG and native observation ownership remain
unchanged. Generic core_math extraction is deferred; geometry-specific reference
and stress policy stay app-owned. No shared API/version/adoption changes occurred.

## What the refinement tests actually establish

Several apparently converged allocations fail independent checks. Broad second
edge-bisection passes exceed the unchanged 50000-tetrahedron cap. Coarsened gap3
and shortened-X gap4 allocations have small last changes but fail physical
surface/weak-reaction agreement. They are retained as negative controls.

Preserving fine X-normal intervals gives a better long-domain candidate. Matching
those physical X nodes instead of rescaling them with duct length also improves
the short-domain candidate. Their last-grid checks are:

| Core reference check | L4 matched | L8 directional | Original limit |
|---|---:|---:|---:|
| Separate pressure-force change | 0.270% | 0.447% | 1% |
| Raw symmetric viscous-force change | 0.507% | 0.885% | 1% |
| Physical surface total vs weak reaction | 0.799% | 0.674% | 1% |

These core checks pass. **The references are still not robust enough to certify.**
Inserting a node at half the first physical X-normal interval, while keeping
all existing X nodes, gives a genuine normal-direction refinement control:

| Genuine normal refinement | L4 pressure | L4 raw viscous | L8 pressure | L8 raw viscous |
|---|---:|---:|---:|---:|
| Gamma0 | 1.438% | 4.880% | 1.611% | 5.454% |
| Consistent gamma=mu | 1.778% | 2.792% | 2.014% | 3.237% |
| Required robustness limit | 1% | 1% | 1% | 1% |

The strongest gamma=10 mu final controls fail physical surface/stabilized-reaction
agreement (L4 1.545%; L8 see audit). Physical, unstabilized weak and added
stabilization reactions remain distinct. No favorable coefficient, projected
normal stress or unstabilized reaction was substituted to obtain a pass.

A flat stationary no-slip incompressible face has zero normal velocity derivative.
Discrete P2/P1 fields retain nonzero normal stress. Much of the previous raw-force
refinement movement comes from that contribution, while tangential wall force
is more stable. This is consistent with pressure/velocity/divergence approximation
and mesh anisotropy sensitivity, not a quadrature defect. We have not proved an
inf-sup instability or a spectral mechanism. Small true residual, empty-channel
calibration and small final-grid force differences are insufficient evidence of
independent obstacle traction accuracy.

Both matched empty references still pass continuous Fourier Pin/D within 1%.
Their usefulness as unit/boundary calibration is preserved; they cannot qualify
corner stresses on their own.

## Pressure-force attribution on both lengths

The following deficits are relative to the still unresolved candidate references;
they are force-functional estimates, not a full-field norm or accuracy certificate.
The decomposition is exact for each fixed reference representation:

native - surface = (projected native trace - surface)
+ (native - projected native trace).

| Finest admitted native run | Trace reconstruction deficit | Solved-pressure functional deficit | Total pressure-force deficit |
|---|---:|---:|---:|
| L4, n48 | 2.582% | 2.359% | 4.942% |
| L8, n40 | 2.429% | 3.276% | 5.704% |

**Both sources matter.** Pressure reconstruction is exact on the quadratic
controls, but loses accuracy for the steep near-body pressure variation represented
by the reference. The native solved-pressure functional supplies the remainder.
The trace error is not monotonic across all intermediate native grids; neither
fixed second-order convergence nor an asymptotic regime is established.

The long native pressure-force screen remains outside 5%. The short screen is
just inside 5% against this candidate, and changes with the unresolved reference.
Do not certify either by choosing that candidate. All native velocity/pressure
fields, finest physical momentum/energy budgets and original distance tests remain
exactly as established by the previous correction. No experimental reference was
installed into the native solver or agent runtime.

## Evidence and cost

Current native worker remains
`c2a234ff0238f12b64c739ccbb1f6304e2ff3244613b125eb453fd40d08caf64`.
The audit verifies native C/header sources and that executable are byte-preserved;
all eight prior exact-worker field readbacks are inherited and their artifact
hashes rechecked. The original seven field binaries remain identical to the
pre-correction predecessor. Preserved 2D/C3D-1..7 numerical kernels and existing
session contracts are unaffected. No new GUI build or interaction was needed
for this reference/test-only change; prior source GUI/regression proof is inherited,
not presented as a newly executed Desktop test.

55 bounded reference attempts produced 51 numerically converged references and
four retained empty-mesh cap rejections. Count-only edge-refinement rejections
are retained separately. Summed reference child wall cost was 1778.30 s (29.64
minutes), not elapsed time: some local jobs overlapped. Maximum observed own-child
RSS was 1578450944 bytes (1505.33 MiB), within 1800 MiB. Admitted body references
stay <=49776 tetrahedra; individual deadlines remain 180 s. Pressure projections
are seconds each, with compact pressure snapshots; this evidence root was about
10.7 MiB before the final audit. Measurements are not isolated benchmarks.

Independent measurement tests (three), native pressure tests (quadratic/gauge and
limited-regularity controls), and focused ASan/UBSan pass. Test-only argument and
fractional-power geometric-roundoff failures were repaired; their logs are retained.
No production defect or acceptance limit was hidden by those fixture repairs.

```sh
make test-cfd-obstacle3d-reference-measurement
make test-cfd-obstacle3d-pressure-trace
make audit-cfd-obstacle3d-reference-refinement
# Expected exit 1 at this unresolved physical gate:
python3 scripts/audit_cfd_obstacle3d_reference_refinement.py --require-qualified
```

Reproduction commands and the declared progression live in the experiment
contract. Reference scripts reuse immutable successful/failed receipts; they do
not automatically repeat failed jobs. `build/c3d-obstacle/refinement-v2/completion-audit.json`
and root `build/c3d-obstacle/completion-audit.json` bind source/test/evidence hashes,
all numerical/cost receipts, robustness failures and signed pressure attribution.
Single-run agent assessment remains reference accuracy not established.

## Next bounded implementation

Stop grading/penalty sweeps in this slice. Establish an independent reference
with a velocity/pressure discretization that enforces divergence accurately enough
to pass genuine normal-direction refinement and raw flat-wall stress identities.
A divergence-conforming finite-element reference is a justified candidate, not
a claimed implemented solution. First prove a small known-answer channel and
no-slip polynomial/corner traction slice, pressure gauge, open traction, true
residual and cost, then apply it to this same L4/L8 cube contract. Retain the
P2/P1 references as comparative failed controls. Do not qualify by projecting
normal stress away or replacing physical traction with a weak reaction.

This recommendation follows the measured failure above. Relevant primary method
literature includes [divergence-free stable elements on barycentric refinements](https://arxiv.org/abs/1710.08044)
and [low-order divergence-free 3D split elements](https://arxiv.org/abs/2105.09214).
Those results establish possible discretizations, not this program's acceptance
or a guaranteed implementation cost. A bounded element/support/cap audit should
choose the smallest viable reference slice before another large obstacle matrix.
The current surface assembly uses [scikit-fem's documented basis/facet interfaces](https://scikit-fem.readthedocs.io/en/stable/api.html).

After the reference passes, use this projection pipeline to evaluate a pressure
trace suited to limited regularity and improve native coupled boundary/pressure
accuracy separately. Prove those changes against known-answer fields and the
independent reference; do not choose an extrapolation order to fit drag. Preserve
all eight controls and original component/energy/distance gates before C3D-8
qualification. Stop before native adaptive grids, STL/motion, inertial/turbulent
wakes, commit or packaging.

Continuation: `/c3d-next Read docs/cfd_obstacle3d_reference_refinement_checkpoint.md
and its goal in Main Edit. Implement the smallest divergence-conforming independent
reference verification slice: known-answer channel, no-slip raw traction/normal
stress, genuine normal refinement, cost and residual gates. Select a viable element
and mesh under existing caps before another cube matrix. Preserve all P2/P1 failed
controls and native/agent baselines. No commit or package.`
