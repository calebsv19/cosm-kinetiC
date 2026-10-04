# Precision-only preconditioning admits the previously blocked force refinement

The optional coupled Float preconditioner plus float64 flexible outer iteration
passes the original, finer base, normal and28416-tet L8 end-slab-refined cube.
The exact end-slab mesh formerly rejected at2254MiB now completes under unchanged
1800MiB/180s/3000iteration/50000tet limits. This enables another actual force test;
physical/native certification and Stage1 remain open.

## Equations, precision and admission

Every original mixed velocity/coupling/pressure coefficient/action/load remains
float64. No physical mode/equation is removed, no pressure shift/diagonal floor or
scaling is introduced. Only separately owned preconditioner values/factor/inner
solve use float32. Public installed SDK caller-owned16-byte factor/scratch APIs
are frozen and strictC11 compiled; opaque internals are not inspected.

Float arithmetic is not claimed to be an exact linear SPD inverse in float64.
Fixed restart60 flexible right-preconditioned Arnoldi stores each actual
preconditioned vector, uses two-pass orthogonalization and checks the true
float64 retained residual at least every10iterations and every restart/termination.
An upper triangular approximate mixed inverse uses Float velocity and negative
exact macro-constant pressure mass solely in the preconditioner. Full reconstructed
originalP4/DG-P3 FE residual/divergence/flux/energy remain publication authority.
All accepted fields meet requested full1e-10 as well as existing numerical gates.

Symbolic/current-RSS numerical admission includes exact Float factor/scratch,
separate rounded input residency, both flexible basis arrays/work reservation and
32MiB reserve, repeated before numerical allocation. API released-byte report is0;
current/sampled/owned peak values retain distinct meanings. Bitwise original input/
pressure/load and FE metadata/catalog restoration checks remain in force.

Eleven controls pass: dense approximate accuracy/repeats/ownership, anisotropic
and refined fullFE/nonzeroRHS/pressure/reconstruction, tight float64 flexible
correction, nonpositive/rounding/invalid/closed/partial/100 cleanup, fitting/rejected
symbolic-only admission, indefinite all-mode/varying action/restart/cap/rank-loss/
nonfinite/zeroRHS/true-metric termination. Initial support failures are retained:
one-step Float error on an anisotropic fixture was larger than assumed (tight
flexible correction proved instead); an unconstrained refined fixture correctly
failed Cholesky until its existing physical wall constraints were applied.

## Complete measured results

| Case | Iterations | Supervisor time | Owned peak MiB | Independent full FE residual |
| --- | ---: | ---: | ---: | ---: |
| Original L4 mixed, chunk128 |120|14.125s|428.641|1.8967e-11|
| Count6 base L4 mixed, chunk512 |120|55.702s|1055.016|5.4572e-11|
| Matched count6 base exact, chunk512 |160|59.841s|1153.125|requested target met|
| Count6 normal L4 mixed, chunk512 |120|70.285s|1289.813|6.7696e-11|
| Count6 normal L8 outer2 mixed, chunk512 |180|102.183s|1445.125|6.1584e-11|

Original mixed is12.1% faster but11.2% more memory than its historical exact
catalog control. Do not make this a universal default. The serial same-host
count6 base chunk512 precision/outer-iteration comparison is~6.9% faster and~8.5%
less owned memory; same chunk/mesh/flow/quadrature/full target. Historical normal
exact97.77s/1555MiB is also retained, but serial measurements do not establish a
universal performance guarantee. Adopt the optional path only for measured,
fully accepted reference meshes, retaining the exact path and all prior failures.

The first base mixed chunk128 attempt is rejected at180.118s during physical
diagnostics, sampled980.891MiB, no field/result. Metadata restored38.992s and
fullFE phase102.916s; no completed full-target/physical/publication claim is made
for that interrupted run. Distinct declared chunk512 experiment preserves the
same quadrature/formulas; the exact same-batch control separates batching from
precision. Do not attribute the whole128→512 cost change to Float factors.

Same-geometry complete matrix/free/RHS identities and accepted field/force
comparisons pass. Largest measured pressure coefficient difference against exact
anchors is4.18e-7Pa, velocity7.47e-11m/s, component/scalar relative changes<7e-10.
These numerical equivalence results do not replace physical force convergence.

## Newly resumed end-slab force test

Exact stopped end-slab mesh/free/RHS/all float64 block identities match.
Float factor873198472bytes and numeric scratch33556391bytes versus previous
Double factor1745502112bytes. Stage reserve includes205437224bytes flexible
basis/work and32MiB; estimate1686746199bytes (~1608.61MiB) admits the attempt.
Repeated numerical live guard and completed run also pass.

End outer1→outer2 pressure/viscous/reaction changes1.635%/.525%/1.203%, inlet
pressure/dissipation .691%/.639%. Maximum Jacobian condition improves543→272.
Raw surface/reaction mismatch2.073%→2.104% stays above1%. The geometry materially
affects pressure/reaction forces, and one cut does not prove an end-mesh plateau.

With inner/body/cross section/flow held, L4 normal→L8 outer2 pressure/viscous/
reaction changes .286%/.055%/.198%, much smaller than stretched-end L8's
1.953%/.583%/1.418%. End subdivisions differ; this is an observation, not a
certified matched mesh/domain limit. Length-dependent pressure/dissipation are
recorded separately, not flat domain criteria.

End field flux error2.533e-10, max divergence1.362e-9/s and energy imbalance
6.488e-11 pass numerical gates. Read-only stress observer completes96.54s/
~553MiB; signed identities and original weak/raw loads are verified independently.
Global strong-equilibrium/jump norms improve .40364/.04250→.21625/.02752.
Smaller global indicators are not a force certificate or proven error bound.

## Next physical controls and evidence

1. Stage-screen one further end-slab cut with identical inner mesh/body/flow and
   the same Float+flexible reserve. Require separate component/scalar/raw force
   convergence and reject allocation if over cap. Preserve this accepted anchor.
2. Add signed force-specific cell/face attribution around .025-.10m edge
   neighborhoods and surrounding cells; screen conforming quality-aware local
   refinement before allocation and measure actual raw/pressure/viscous/reaction
   improvement. Keep cancellation, both lifts and independent fullFE authority.
3. Native stationary aligned-box qualification follows independent physical
   reference gates; authored boxes/transient/outlet/wake and general objects follow
   separately. Do not promote reference cost/field acceptance as native certification.

Evidence: build/c3d-mixed-precision/checkpoint-audit.json, support-test-receipt.json,
SDK/support build records, frozen runs/control-runs/stage-runs/observer-runs.
Goal: cfd_3d_mixed_precision_goal.md and declared execution/batching controls in
cfd_3d_mixed_precision_execution_goal.md. Reproduce proofs with
`make test-cfd-reference3d-mixed-precision`; declared numerical commands retain
explicit chunk512/count/domain arguments in their receipts. Native/shared API,
versions, commits, packages and installed app are unchanged by this slice.
