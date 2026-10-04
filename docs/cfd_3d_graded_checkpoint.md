# Quartic stress attribution, mesh conditioning and acceptance correction

2026-10-03, PhysicsSim Main Edit. The independent quartic stress observer and
all-macro pressure-null checks are verified. Two mesh-conditioning experiments
improve linear convergence but fail the original raw-force gates. The evidence
audit also catches and preserves one divergence-failing linear success; the new
reference runner now gates complete numerical acceptance before atomic field
publication. The [broader goal](cfd_3d_graded_goal.md) remains active. The cube
reference, native force accuracy and useful object/wind-tunnel qualification
remain incomplete.

## Independent stress and pressure diagnostics

Physical P4 velocity Hessians come from an independent 35-node monomial fit and
affine derivative transform. Degree-6 exact volume quadrature and degree-7 exact
facet products give the elementwise identity separately for pressure and symmetric
viscous stress:

`raw surface load - weak lift load = interior stress jump - weighted volume stress divergence`.

Four tests pass: analytic quartic Hessians on an affine sheared mesh; continuous
polynomial stress with no interior jump; arbitrary non-solenoidal velocity and
discontinuous cubic pressure component identities; and pressure-only facet
quadrature agreement. Eight saved-field observations reproduce original raw/weak
loads. Maximum component identity error is 1.61e-14 N. Observers use immutable
input/source receipts; measured wall times are 3.79–12.89 s and observed RSS
291.58–444.56 MiB. They do not alter fields or certify physical accuracy.

| Field | Volume stress-divergence L2 | Interior stress-jump L2 | Mesh-weighted squared score | Raw minus weak force, X |
|---|---:|---:|---:|---:|
| Original quartic | .255263 | .0341771 | .0367726 | -.001468414 N |
| Original body4 | .157668 | .0216685 | .0106360 | -.001201983 N |
| Balanced outer spacing | .0536165 | .0147278 | .00202766 | -.002915457 N |
| Balanced body4 | .165167 | .0299784 | .0133072 | -.001242836 N |
| Outer cells subdivided | .234238 | .0322004 | .0212756 | -.001637817 N |
| Outer subdivision plus tighter normal split | .251167 | .0316814 | .0259448 | -.002740438 N |

On the original body4 field, 77.4% of the h²-weighted volume score lies in cells
whose centroids are farther than .4 m from cube edges. Its highest-scoring macros
include long upstream/downstream cells beside thin transverse layers. Distance
buckets are whole-cell/facet centroid partitions; scores are diagnostics, not
force-error bounds. The balanced mesh demonstrates why smaller unsigned norms
cannot replace raw-force convergence: its norms decrease while its force gap
nearly doubles. Pressure and viscous weak parts depend on the lift and are not
replacement physical drag components.

Every tested solver mesh checks all local macro bubble pressure ranks. The exact
original cube has 1,248 macros, each rank 79 out of 80 pressure DOFs, with its
constant local mode null to about 1e-15. The global macro-constant diagonal-velocity
Schur restriction has no eigenvalue below 1e-12; its smallest checked eigenvalue
is .00212199. Combined checks give numerical evidence against extra exact
assembled pressure null directions. They are not a full pressure-space condition
number, mesh-independent inf-sup theorem or force qualification.

## Physical spacing controls and retained failures

The first control replaces the original cubic outer-axis spacing with balanced
spacing, preserving original topology, cube, domain, symmetry and body-plane
nodes. Its maximum affine Jacobian condition falls from 215.37 to 31.64 and its
minimum positive local pressure eigenvalue rises from 8.23e-5 to .00366574. The
second control subdivides only the long outer X intervals into three pieces,
preserving every original axis plane and all near-body/transverse spacing. Its
maximum geometric condition is 82.33. Support tests verify volume, exterior/body
areas, true boundaries, symmetry, affine Alfeld centers and actual spatial
subdivision; no false internal wall is introduced.

| L4 control | Tets | Iterations | Wall s | Observed RSS MiB | Raw/reaction mismatch |
|---|---:|---:|---:|---:|---:|
| Original, all modes | 4,992 | 610 | 31.96 | 975.58 | 3.6436% |
| Balanced spacing | 4,992 | 200 | 14.42 | 966.08 | 7.0964% |
| Balanced normal split | 6,720 | 180 | 19.69 | 1082.11 | 5.5528% |
| Balanced body4 | 10,752 | 270 | 43.82 | 1646.20 | 3.0087% |
| Outer X subdivision | 8,448 | 400 | 44.66 | 1290.36 | 4.0735% |
| Outer subdivision + tighter normal split | 10,176 | 483 | 64.79 | 1529.00 | 6.8236% |

All physical comparisons fail the unchanged 1% gates. Balanced normal subdivision
changes viscous force by 1.225% and reaction by 1.579%; balanced body4 changes
pressure by 6.540% and viscous force by 1.994%. Preserving the original near-body
spacing and refining outer cells still leaves normal pressure sensitivity of
4.780%. Faster solves and improved conditioning are measured improvements, but
neither mesh is adopted as a qualified force reference. The final runner defaults
to original outer grading. Full tests at L8 are deferred until L4 passes rather
than using the prior empty-L8 calibration as cube evidence.

## Corrected numerical acceptance and publication

The initial outer-refined normal solve has true residual 8.45e-11 but maximum
physical divergence 1.17005e-8, above the unchanged 1e-8 gate. Its frozen old runner
labels linear success as `numerically_accepted`; the new audit explicitly rejects
that full-acceptance claim. The immutable receipt, snapshot, observer and first
failing audit log remain intact. This field is diagnostic evidence of a failed
numerical control, not an accepted reference. An observer identity can hold on
such approximate fields without proving their numerical acceptance.

The tighter rerun reaches residual 3.85e-12 and maximum divergence 2.22e-10, passing
all numerical gates. MINRES stops internally before the requested 1e-12 target;
that target is explicitly not attained. Separate forces change by <3e-8 relative,
so the physical force failure is independent of this marginal divergence issue.

The new graded reference runner separates `linear_solve_accepted` from complete
`numerically_accepted`, reports failed gate names, and checks flux, whole-field
maximum divergence, energy, residual and resources before snapshot publication.
Serialization writes an owned pending file, then checks resources again before
atomic rename; a rejected pending file is removed. The observer now requires
complete input numerical gates before observation. A separate retained rejection
control confirms the known divergence-failing input stops before publishing any
diagnostic. Earlier frozen sources and
receipts remain unchanged and can be interpreted using the stricter audit.

Two regression controls use the observed divergence failure and a simulated
serialization-time RSS overshoot. Both reject publication as required, and the
successful atomic publication path is checked. A final full original-cube runtime
control has identical geometry/matrix/RHS/free-DOF hashes and identical forces to
the earlier original control, finishes in 32.15 s/982.22 MiB, and publishes only
after all gates pass. This acceptance correction applies to the new reference
runner; native runtime and older reference runner sources are unchanged.

Caps remain 50,000 tets, 1800 MiB own/observed RSS, 180 s and 3000 iterations.
No equation penalty, regularization, smoothing, projected force, weak-force
substitution or resource/accuracy relaxation is introduced.

## Next implementation and remaining integration

The next bounded backend improvement is exact local condensation and field
reconstruction. Prove its algebra on polynomial controls and the matched original
cube, including reconstructed full residuals and raw stress/reaction forces,
before claiming memory savings. Only measured savings can admit finer cube
stress-resolution controls under the existing caps. This is preferable to
another unsupported batch-size reduction or mesh chosen for favorable signed
cancellation. Retain the higher-order resource failures and all rejected meshes.

Then qualify raw pressure, viscous, reaction and energy convergence on L4 and L8.
Use the qualified reference to correct native pressure/traction, prove authored
stationary objects, and continue transient/outlet and inertial wind-tunnel
validation. Curved/moving bodies and general object/wake certification remain
later requirements; this diagnostic slice does not complete that objective.

Reproduction: `make test-cfd-reference3d-graded` runs four observer and six
mesh/mode/acceptance tests. `make audit-cfd-3d-graded` verifies immutable sources,
receipts, fields, conservation, modes and explicit physical failures. Evidence is
in `build/c3d-graded/checkpoint-audit.json`, separate test logs, `runs/`,
`observer-runs/` and `audit-rejected-divergence.log`. Native sources, predecessor
workers, fields and audits are preserved. No commit, package, installation or
canonical adoption occurred.
