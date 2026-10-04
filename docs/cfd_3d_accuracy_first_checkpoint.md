# Accuracy-first reference testing resumed

2026-10-04, existing PhysicsSim Main Edit. The user's priority is accurate CFD
behavior before execution speed. A separate declared local contract of 3072 MiB,
600 seconds, 50000 tetrahedra and 3000 iterations restores physical testing on the
verified 16 GiB host. Historical 1800 MiB/180 s controls and their cost rejections
remain immutable. The 23.50/106.64 s and 300 MiB saving thresholds do not gate this
new accuracy lane. Full original FE target 1e-10, retained target 1e-11, flux and
maximum divergence 1e-8, per-run energy balance 3%, separate force convergence 1%
and raw surface/reaction agreement 1% remain.

The previously qualified complete Float Cholesky preconditioner acts against the
original complete Double P4/DG-P3 equations. Original physical bits, complete
pressure modes, both flexible bases, fresh factor/scratch/work admission, completed
owner retirement and atomic field publication remain checked. No new performance
preconditioner was needed to obtain these finer physical measurements.

| Accepted finer field | L4 | L8 |
|---|---:|---:|
| Tetrahedra | 43008 | 49920 |
| Iterations | 84 | 130 |
| Whole process seconds | 154.383 | 231.207 |
| Owned peak MiB | 1933.078 | 2456.672 |
| Independent full FE residual | 6.035e-12 | 8.893e-12 |
| Maximum divergence /s | 1.451e-10 | 2.412e-10 |
| Relative physical energy imbalance | 1.929e-11 | 2.446e-11 |
| Pressure-force change versus prior same-length mesh | 0.158% | 0.297% |
| Raw viscous-force change | 0.160% | 0.173% |
| Reaction-force change | 0.208% | 0.283% |
| Inlet-pressure change | 0.156% | 0.167% |
| Total-dissipation change | 0.175% | 0.169% |
| Raw surface/reaction mismatch | 1.7215% | 1.7265% |

Pressure, viscous and reaction domain sensitivities on this fine pair are
0.1584%, 0.0443% and 0.1220%. Separate force refinement and domain checks pass on
these comparisons. The original raw traction equilibrium check still fails.
Two numerically accepted fields therefore resume convergence testing without
certifying obstacle force accuracy or a mesh limit.

## Correct domain interpretation

The earlier authoritative [domain checkpoint](cfd_3d_domain_checkpoint.md) explicitly
distinguishes length-dependent inlet pressure and total dissipation from body-force
domain sensitivity. Some later summaries incorrectly described their approximately
25% L4-to-L8 changes as a global scalar physical failure. This assessment corrects
that interpretation; historical sealed reports are retained.

The new fine pair reports pressure/dissipation changes of 25.597%/25.482%. A longer
no-slip duct dissipates more power and requires more pressure at the same flow.
Neither scalar is required to remain flat under a change of physical length.
Both stay visible, without subtraction or replacement. Fixed-length scalar mesh
convergence and each run's physical energy balance remain checked. Existing analytic
empty-duct conductance supplies the independent length-scaling control.

## Stress localization and next accuracy experiment

An independent signed full-stress observer completes in 254.320 seconds, sampled
508.625 MiB. It reconstructs the accepted L4 coefficients, checks both lift identities
to 4.108e-14 N, retains separate pressure/viscous volume and interior-jump arrays,
and recovers the -0.683054 mN raw-minus-weak drag gap.

The innermost 0.025 m centroid band contributes -0.666956/-0.660324 mN under the two
lifts. Highest individual scores occur about 0.020 m from cube edges, on side-face
fluid cells. Larger positive and negative contributions cancel elsewhere; centroid
bins and scores are diagnostics, not clipped-region identities or force-error
bounds. Boundary divergence and normal viscous corrections are already tiny; weak
volume loads agree with reaction. Replacing raw traction with those loads would
conceal the unresolved stress-trace error.

The next bounded accuracy test should resolve those first side-face fluid layers,
retain actual body triangles, measure conditioning and all physical outputs, and
compare separate forces against these saved fine fields. More linear iterations
or preconditioner tuning is not the next priority given strict residuals already
below 1e-11. A normal-layer redistribution is a diagnostic control, not uniform
refinement; if useful it requires a further independent spatial/domain check.

After reference qualification, return to the original wider CFD sequence: isolate
native solved-pressure/operator versus reconstruction/traction errors through exact
projection; qualify native force, energy and domain tests; then implement reusable
stationary-object scene contracts and physical low-Re transient transport. The
current reference is steady Stokes. Advection, inertia, wakes, moving bodies,
turbulence and free surfaces require explicit models and their own known-answer
and convergence tests. Fast completion of a steady solve cannot certify them.

## Evidence and verification

Five resource/publication/geometry/recipe controls and four independent physical
comparison/observer controls pass. `build/c3d-accuracy-first/physical-assessment.json`
records source-bound coarse/fine and domain comparisons. Frozen solver and observer
receipts, saved fields and signed arrays remain under this root; the once-only
`checkpoint-audit.json` seals them. Protected native binary/audit hashes stay intact.
Only current-truth and README preexisting documents change. No commit, package,
installation, canonical adoption or deployment occurs. The broad CFD goal remains
active and incomplete.
