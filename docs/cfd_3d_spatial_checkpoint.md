# Spatial controls and reference memory improvement

2026-10-03, existing PhysicsSim Main Edit. Force-convergence testing is running
again under the original caps. The useful integrated change is a lower-memory
reference backend. Larger normal and cube-edge controls now finish, but raw
pressure traction remains spatially sensitive. **The physical reference remains
unqualified, stage 1 remains open, and the broader object/wind-tunnel goal remains
active.** See the [slice contract](cfd_3d_spatial_goal.md) and
[prior preconditioner result](cfd_3d_preconditioner_checkpoint.md).

## Useful backend change

The working numerical action remains sparse factorization of the scalar velocity
block, reused across all three components, plus the exact DG pressure mass inverse.
The physical P3/DG-P2 Alfeld Stokes forms, natural ends, cube/domain geometry, fixed
flow, stress observer, residual requirements and resource limits are unchanged.

New app-owned reference helpers assemble and observe cells in batches, keep only
small metadata bases between batches, stream matrix hashes, retain compact body
reaction weights, and apply the saddle operator directly from scalar stiffness
and divergence blocks. They avoid storing three stiffness copies and both global
saddle off-diagonal copies. The reaction remains a diagnostic; it does not replace
raw surface pressure or symmetric viscous force.

On the identical 13,824-tet control, peak observed own-process RSS decreases from
1,317.30 MiB to 822.59 MiB (37.55%). Same-mesh force and scalar relative changes
are below 1e-7; actual pressure-force change is 3.62e-10 relative. Maximum field
coefficient changes are 1.66e-12 for velocity and 3.68e-9 Pa for pressure. These
are measured floating-point differences, not bit-identical fields. The implicit
operator's canonical logical CSR digest exactly matches explicit CSR on the same
chunk assembly. Full-basis versus chunk assembly can differ in last-bit entry
sums; tests compare their actual entries and independent field/force results.

Chunking alone did not fit the larger control. Two 23,616-tet attempts exceeded
RSS during assembly, at observed 1,856.84 and 2,019.14 MiB. Both failed receipts
and frozen sources are retained, with no accepted field. The implicit saddle
path completes that case at 1,021.20 MiB. Its own reported peak is 1,026.47 MiB.
Process sampling can miss brief peaks; the new probe also checks its own peak.

A CSR stacking experiment preserved entries but did not improve measured peak
memory over the earlier lean staging control. It is retained for comparison;
the useful memory path is the implicit saddle operator.

## Physical controls

All children retain 50,000-tet, 1,800-MiB, 180-s and 3,000-iteration caps. Final
true residual must be <1e-8; ordinary early target remains <=1e-10. These
controls reached the early target. Whole-face order-4/order-8 traction quadrature
agrees; raw normal viscous force is measured and small without forcing it to zero.

| L4 control | Tetrahedra | Iterations | Wall time | Observed own RSS |
|---|---:|---:|---:|---:|
| Body4, inserted 0.03-m planes, full storage | 13,824 | 630 | 28.88 s | 1,317.30 MiB |
| Same body4, implicit saddle | 13,824 | 630 | 34.30 s | 822.59 MiB |
| Body6, implicit saddle | 23,616 | 620 | 79.57 s | 1,021.20 MiB |
| Body6, first-normal split | 28,416 | 930 | 118.43 s | 1,186.28 MiB |
| Body6, one edge refinement within 0.04 m | 28,224 | 650 | 75.56 s | 1,173.83 MiB |

The memory change costs about 5.4 s on the matched body4 control but enables
previously memory-blocked physical tests. Individual wall times are local
measurements, not a throughput/scalability guarantee.

The initial topology-preserving X-axis warp also moved outer spacing and changed
weak reaction substantially. That mesh is not adopted. Inserted normal planes
preserve every original outer structured axis node and all Y/Z nodes. The normal
split adds two X intervals. Edge refinement subdivides the macro mesh conformingly
before Alfeld splitting; mirrored interfaces are checked for cracks/false walls.
Its axis metadata describes the background structured axes, not every new adaptive
vertex or Alfeld center.

| Refinement | Pressure change | Raw viscous change | Reaction change | Raw surface/reaction mismatch, base → refined |
|---|---:|---:|---:|---:|
| Body4 first-normal split | 17.195% | 0.588% | 0.056% | 2.932% → 14.554% |
| Body4 → body6 | 8.246% | 3.157% | 0.307% | 2.932% → 1.398% |
| Body6 first-normal split | 8.975% | 0.830% | 0.045% | 1.398% → 3.974% |
| Body6 edge refinement | 2.944% | 0.080% | 0.318% | 1.398% → 2.934% |

The body6 normal case has true residual 8.29e-11 and maximum measured volume
divergence 3.61e-9 s^-1. Flux and energy checks pass, but pressure-component and
raw/reaction <=1% gates fail. The edge case has residual 6.65e-11 and energy
imbalance 7.91e-11; it also fails the physical force gate. Neither mesh is promoted
as a qualified force reference. L8 robustness is still required and is not claimed.

Pressure difference on the body6 normal pair is 0.002307 N, split almost equally
between front/back faces. Existing masked-quadrature edge bins show oscillating
signed contributions, especially within 0.1 m of edges. Those bins are diagnostic
sampling partitions, not exact geometrically cut integrals or an error estimator.
Whole-face integrals are quadrature checked.

The implicit empty-duct control retains independent Fourier calibration:
pressure error 0.01258%, dissipation error 0.01666%. Its wall time is 2.94 s and
observed RSS 252.09 MiB. Empty calibration does not certify obstacle traction.

## Pressure modes and rejected preconditioners

The exact original 4,992-tet cube is rechecked using all 1,248 local macro pressure
blocks, not just the previously checked global macro constants. Each macro has
40 pressure coefficients and 45 interior velocity-bubble coefficients. Every
local bubble Schur block has rank 39 at the recorded threshold; its one missing
mode is the constant, with relative B-transpose constant error <=6.32e-16.
Positive generalized eigenvalues range from 8.9995e-5 to 0.99928.

The global macro-constant restriction again has smallest eigenvalues
0.003091–0.009557 and nonzero constant-pressure gradient action at natural ends.
Local mean-zero rank plus injective global constants gives numerical evidence
against an additional exact pressure null direction on this assembled cube. It
is not a uniform inf-sup bound, a full-space spectral condition estimate, or a
pressure-trace accuracy proof. The intended local/global decomposition is
consistent with [Guzmán and Neilan's barycentric-refinement analysis](https://arxiv.org/abs/1710.08044),
which treats an interior-point split and the degree-k >= dimension pair.

Jacobian condition indicators on the original cube remain median 19.37, maximum
215.37. The normal-refined body6 maximum is 247.70; the edge case has median 17.88
and maximum 215.37. These depend on vertex ordering and do not alone certify mesh
quality or explain force error.

Two pressure actions preserve exactly the same logical matrix/RHS/mesh/free-DOF
hashes. Both pass independent symmetry, positive-definiteness, viscosity-scaling
and known mixed-solution controls. Actual cube testing rejects them as improvements:

| Exact cube pressure action | Iterations | True residual | Wall time | Observed own RSS |
|---|---:|---:|---:|---:|
| Working mass inverse | 710 | 9.45e-11 | 9.97 s | 337.23 MiB |
| Local bubble Schur + global constants | 3,000 | 3.74e-8 | 39.74 s | 370.84 MiB |
| Interface-aware Schur blocks + global constants | 1,460 | 9.59e-11 | 19.89 s | 378.66 MiB |

The first candidate produces no accepted field. The second reproduces force but
roughly doubles cost. Neither is adopted; mass remains the default. The explicit
`--pressure-kind macro/patch` options are experimental retained controls, not
recommended solver settings. Pressure mode checks alone do not choose a useful
preconditioner. The rank-one completion used internally by the bubble action is
projected out of its local inverse and never added to the physical Stokes matrix.

## Verification and next continuation

Sixteen focused tests pass: legacy/geometry/edge mesh controls; entry, mass,
energy, non-solenoidal lift, compact reaction and implicit operator controls;
pressure-rank detection including deliberately broken coupling; and both
experimental preconditioner support controls. The audit verifies every failed and
successful receipt, frozen source/supervisor, resource contract, finite snapshot,
field/force comparison, predecessor artifact and original worker hash. All
pre-existing source/headers/helpers remain unchanged; only docs/current-truth/index
and Make test/audit targets change alongside new app-owned reference files.

`build/c3d-spatial/checkpoint-audit.json` records
`persistent_goal_complete=false`, `stage_1_complete=false`,
`physical_accuracy_certified=false`, and `experimental_pressure_candidates_adopted=false`.
No native/GUI regression matrix was repeated for these independent reference
changes. No commit, package, installation, canonical adoption or shared API/version
change occurred.

```sh
make test-cfd-reference3d-spatial
make audit-cfd-3d-spatial
python3 scripts/run_cfd_reference3d_spatial.py --name NEXT-CONTROL -- --kind factor --count 6 --normal-spacing .03 --insert-normal --chunk-size 1024 --matrix-free
```

The last command is an explicit local-development child, not public worker or
native session admission. It writes immutable source-bundled receipts under this
checkout. Use a fresh descriptive name for a new control. Identical retained
commands/artifacts are verified and reused; mismatched reuse is rejected.

The next bounded diagnostic is an equilibrium-defect attribution of raw pressure
trace versus weak reaction: measure volume momentum defects and interior stress
jumps near cube edges/normal layers with an independently verified observer.
Use that evidence to choose one conforming targeted refinement, then require
separate pressure/viscous/reaction/scalar <=1% controls on L4 and L8. Preserve the
unmodified raw Cauchy traction gate; do not substitute a weak/recovered force or
smooth pressure simply to pass. If cost becomes the obstacle, improve measured
assembly/observation or velocity-preconditioner cost under the existing caps.

Once that reference gate passes, resume native pressure/traction comparisons and
a stationary authored-box scene/session contract, then physical transient/outlet
and wake diagnostics. Broader geometry, moving bodies and coupled atmosphere are
later qualification gates. The current blocker is accurate cube pressure traction,
not source-checkout operation or an unverified remote/public workflow.
