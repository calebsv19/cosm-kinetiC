# Useful paired pressure correction; matched L8 memory admission withheld

Four independent algebra controls pass after a pre-numeric matrix-RHS broadcast
correction (initial failed log preserved). The new balanced mass/coarse pressure
inverse retains all physical pressure directions and includes ten complete quadratic
coarse polynomials, including constant pressure. Coarse Schur columns use the
existing coupled Float velocity inverse and unchanged condensed pressure block.
Only the preconditioner changes; complete float64 mixed equations, mesh/load/modes,
exact reconstruction and requested full1e-10 remain authoritative.

| Paired L4 control | Mass | Quadratic correction |
|---|---:|---:|
| Original iterations |260|216|
| Original whole seconds |12.200|11.630|
| Original owned MiB |421.578|366.344|
| Normal iterations |290|216|
| Normal whole seconds |86.438|73.074|
| Normal owned MiB |1187.844|1247.016|

Both fields in both pairs pass all numerical publication checks, retained1e-11,
full1e-10, flux/div1e-8, energy.03 and original caps. Exact mesh/operator/load and
Float-input identities match between pairs and prior sealed controls. Separate
pressure/rawviscous/reaction/Pin/D changes are below1.5e-10, preserving failed raw
force consistency (original3.644%, normal2.108%). Quadratic iterations improve
16.9%/25.5%; setup+solve improves9.81%/25.28%; whole improves4.67%/15.46%.
Normal owned peak increases4.98%; original decreases13.10% in this pair. Disclose
allocator/run variability; do not infer general memory reduction. Coarse skew and
reproduction errors are separately gated<1e-5. These are coarse diagnostics, not
complete DG nullspace, inf-sup, coefficient forward-error or physical certificates.

Both paired cost/equivalence gates passed before the one predeclared matched run.
Exact33216tet L8 second-normal held-outer2 mesh/operator/load identities match the
prior accepted field. This attempt stops before numeric factorization, solve or
field publication because admission requires1831.051MiB>1800MiB. Whole diagnostic
22.958s, sampled1345.188MiB; the sampled peak is pre-numeric, not proof a complete
factor/field would fit. No iteration/residual/force result exists for this attempt.

Admission components MiB: factor1073.836, numeric scratch36.950, measured post-relief
resident639.641, reserve32, original both-basis/work37.349, additional coarse11.274.
The previously accepted matched mass run measured543.906MiB at this residency
point, with1724.042MiB admission. Current difference is95.734MiB in measured
residency plus11.274MiB added reserve. Coarse arrays have not yet been built.
Even mass-only reservation at this measured residency would require1819.777MiB.
Therefore the failure is not evidence that allocated coarse arrays caused the
entire gap, and no fresh mass rerun is inferred or silently substituted.

Per the predeclared scope, the correction remains experimental: no complete useful
matched result, so no adopted optional global correction/default/native change.
Existing accepted L8 field and force tests remain available. Original cube1.757%
raw/reaction mismatch and raw total pressure/dissipation domain failures stay open.
Eight preceding body remesh quality rejections remain sealed. Next measure and
bound residency on this exact mesh, including RSS measurement timing after live
validation, before any distinct storage/control continuation. Do not retry this
failed numeric artifact or loosen reservations/caps.

Evidence build/c3d-pressure-coarse/runs/7c952ea4c38d24b49fc66f2389833e1516223a0cfe570763ae853127e380eefc,
matched-runs/868662556f48f2a3b293114d154f4c202c364c13b7562b87dfe6b6b8607694c7,
source transforms, support-test-receipt.json, matched-eligibility.json,
matched-control-receipt.json, comparisons.json and checkpoint-audit.json.
No commit/package/installation; broad Stage1/native/general qualification stays active.
