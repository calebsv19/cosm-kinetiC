# Cube traction equilibrium attribution and rejected adaptive mesh

2026-10-03, PhysicsSim Main Edit. The new independent observer explains the saved
raw-surface/weak-reaction gap through elementwise stress divergence and interior
stress jumps. One score-guided physical refinement was then tested and rejected
as an accuracy improvement. The [broader goal](cfd_3d_equilibrium_goal.md) remains
active; stage 1 and the physical cube force reference remain unqualified.

## Observer and independent support

For a scalar discrete lift eta equal to one on the cube and zero at outer
boundaries, elementwise integration by parts gives

`raw body load - weak lift load = interior stress-jump load - weighted volume stress divergence`.

The observer verifies this identity separately for pressure and symmetric viscous
stress. Cubic velocity Hessians are evaluated by exact nodal-polynomial fitting
and an affine coordinate transform, independently of the assembled solver rows.
Volume and facet observations use polynomial-exact quadrature in bounded batches.
Library interior-facet normals have a common side-0 orientation; the stress jump
uses left minus right traces, with the sign checked on arbitrary discontinuous
pressure and non-solenoidal velocity.

Four support tests pass: analytic physical Hessians on a sheared tetrahedral mesh;
smooth global polynomial stress with zero interior jump; arbitrary non-equilibrium
velocity/discontinuous pressure with both component identities; and pressure-only
facet quadrature agreement. Every diagnostic reproduces predecessor raw force and
weak lift before interpreting defects. Existing snapshots, pressure coefficients,
Stokes equations, stress observers and residual/force gates are unchanged.

The pressure and viscous weak parts depend on the chosen lift; they are not
alternative physical drag components. Signed volume and jump terms can cancel.
Whole-cell/facet centroid distance buckets are diagnostic partitions, not exact
geometrically clipped edge bands. Unsigned scores use h² times squared volume
stress-divergence norm and h times squared interior stress-jump norm, split equally
to adjacent cells. They guide experiments; no force-error bound is claimed.

## Measured mechanism

All values below are from the already accepted P3/DG-P2 fields and the 0.25-m
lift. Both lift shells reproduce the total force gap. Maximum component identity
error across the initial three controls is 5.68e-15 N.

| L4 field | Raw minus weak force, X | Volume equilibrium defect L2 | Interior stress-jump L2 | Mesh-scaled squared score |
|---|---:|---:|---:|---:|
| Body6 base, 23,616 tets | -0.000564961 N | 0.282102 | 0.037795 | 0.0314600 |
| First-normal split, 28,416 tets | +0.001606479 N | 0.723205 | 0.062500 | 0.0619295 |
| Edge refinement, 28,224 tets | -0.001181728 N | 0.232021 | 0.030862 | 0.0285305 |

The normal split increases both unsigned defects and changes the signed force
gap. Edge refinement decreases both defect norms yet worsens net force mismatch.
Thus improving an unsigned defect alone cannot replace raw component/reaction
convergence. On the base field, about 84.6% of its h²-weighted volume score lies in
cells whose centroids are farther than 0.4 m from cube edges. Several dominant
macros are long outer-flow cells upstream/downstream, rather than the thin cells
chosen by the preceding edge-distance heuristic.

Observer children use immutable source-bundled receipts and verify input receipt,
snapshot, source, DOF coordinates and mesh identities. All seven children finish
under unchanged 180-s/1800-MiB caps; measured wall times are 24.9–41.7 s and observed
RSS 325–418 MiB. Three pairs with/without score bookkeeping produce identical
force identities and defect norms. Solver residual/iteration receipts remain the
numerical authority for the observed fields; observers do not solve or certify a
new physical field.

## Physical improvement tested

The next mesh experiment groups original per-macro scores into 738 reflection
orbits, each containing eight macros. It reconstructs the exact predecessor
geometry, marks the highest 32 macros in one octant, refines conformingly, mirrors
that octant, and applies the same Alfeld split. This captures 51.53% of the base
score and produces 27,264 tets. Independent mesh tests verify full reflection
symmetry, unchanged cube surface area/domain volume, no false internal walls,
unchanged input snapshots and rejection of input drift.

The existing reference probe gains only an optional explicit mesh-data argument;
the same forms, element, factor/mass actions, natural boundaries and raw force
observers are reused. A default-path control preserves the prior exact-cube
matrix/RHS/mesh/free-DOF identities and forces within 1e-7. This small reference
orchestration change is recorded separately from unchanged native sources.

The adaptive physical solve finishes in 880 iterations, 79.41 s and 1,047.52 MiB
observed RSS. True residual is 9.46e-11, maximum measured divergence 3.42e-9 s^-1,
and energy imbalance 1.66e-11. Its physical changes relative to the body6 base are:

| Quantity | Relative change |
|---|---:|
| Raw pressure force | 1.633% |
| Raw symmetric viscous force | 0.0354% |
| Weak reaction force | 0.2287% |
| Inlet pressure | 0.1650% |
| Dissipation | 0.1691% |

Raw surface/reaction mismatch increases from 1.398% to 2.226%. Re-observation shows
volume defect L2 0.336304, stress-jump L2 0.045574 and squared score 0.0935872; the
targeted score did not decrease. Jacobian condition indicators worsen from median
19.19/maximum 215.37 to median 21.57/maximum 289.21. Those indicators do not prove
causation. This mesh is retained as a failed accuracy control and is not adopted
as a qualified reference or native object solution.

## Evidence and next continuation

Six focused observer/mesh tests and the equilibrium audit pass. The audit verifies
all seven observer receipts, the adaptive physical solve, the default-path control,
all predecessor spatial receipts, preserved fields, original worker hashes and
pre-existing source drift. Only docs/index/current-truth, Make targets, and the
explicit reference mesh-input seam change alongside new app-owned observer and
adaptive experiment files. Native source/headers and existing numerical form,
element, preconditioner, traction and consistency helpers remain unchanged.
No commit, package, installation or canonical adoption occurred.

`build/c3d-equilibrium/checkpoint-audit.json` records
`persistent_goal_complete=false`, `stage_1_complete=false`,
`physical_accuracy_certified=false` and `saved_fields_unchanged=true`.

```sh
make test-cfd-reference3d-equilibrium
make audit-cfd-3d-equilibrium
```

The next bounded investigation should test a higher polynomial approximation
order for velocity and pressure, with independent conformity, divergence-support,
polynomial Stokes/traction, natural-boundary and empty-duct calibration controls
before testing the cube. Preserve the continuous Stokes PDE, physical boundary
contract, raw Cauchy traction, original residuals and resource caps. Keep P3/DG-P2
controls intact and compare actual pressure/viscous/reaction convergence; do not
promote a new method merely because its support tests pass. Mesh conditioning
still needs measurement, and this result does not prove that every possible
P3/DG-P2 mesh fails.

After the reference force gate passes on L4/L8, resume native pressure/traction
comparisons, stationary authored-object contracts and physical transient/outlet
wind-tunnel diagnostics. The broader product goal is not completed by this
observer or by a reference-only numerical control.
