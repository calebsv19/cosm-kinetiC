# Transition quality rejection and measured uniform-cube force control

The broad useful object/wind-tunnel CFD goal remains active. This slice screens
geometry and measures a complete cube field/stress response under unchanged
P4/DG-P3 equations, pressure modes, residual/conservation/publication, physical
force and resource gates. No new mesh receives physical acceptance; native
source/API/version/package/install/commit surfaces remain unchanged.

## Geometry and support

Original inserted edge points stay on shared original edges with original vertices
fixed, Y/Z partners coupled and octants mirrored. Five declared fractions and at
most four coordinate passes are tested on eight force-ranked pairs. All eight
retain midpoint positions after one pass and fail affected worst-shape admission.
Six balanced-cosine count6/count8 controls then vary common normal distance with
two end layers. Several improve global geometry, but all six fail declared
near-edge shape criteria. All fourteen failed candidates remain retained; none
reaches PDE allocation and no gate is relaxed to admit them.

Five support controls prove intrinsic shape at24labels/3scales, shared topology/
original-edge constraints, per-parent volume partition, actual optimization
bounds/refusals and independent original mixed FE action/nonzero-load elimination/
reconstruction. Initial incorrect affected-parent counts are corrected from the
predecessor. An extreme aspect24 arbitrary-load fixture reaches5.96e8pressure
coefficients and4.31e-9absolute mean recovery error; that failure remains recorded.
Moderate aspect2.5 original-action control passes with1.83e-11 recovery error.
These arbitrary algebra fixtures do not certify physical fields or a universal
absolute pressure-recovery/global-pressure-spectrum guarantee. Full physical
residual target remains1e-10.

A distinct uniform-body family uses common first-normal distance1/16m and two X
end layers. Uniform spacing is coarser at the first body edge interval than cosine,
and finer along middle edges; better shape need not imply better forces. Two
additional actual controls prove true boundary vertices/areas, independent
positive determinants/volume15m3, every barycenter/orientation and body/normal
plane metadata. Seven support controls pass in total.

| Geometry | Cosine count6 base | Uniform count6 | Uniform count8 |
| --- | ---: | ---: | ---: |
| Tetrahedra |18816|23616|36096|
| Below.025m centroid-edge worst intrinsic shape |6.077|3.822|2.874|
| Below.025m centroid-edge mean intrinsic shape |3.691|2.974|2.392|
| Global maximum Jacobian condition |215.37|118.10|118.10|

Centroid sets differ; these are not same-parent or clipped-region error bounds.
Count8 symbolic/current/basis estimate 1915.984MiB exceeds1800; no
numeric factor or field. Count6 estimate 1362.048MiB admits, including668542996factor
bytes,28438168scratch,527433728current RSS,170241896basis/work and33554432reserve.
Symbolic cleanup/full-input/action preservation and numerical live admission pass.
Caps remain1800MiB/180s/3000iterations/50000tet.

## Completed force and stress measurements

The count6 uniform cube completes80iterations /62.517s /1339.344MiB with independent
full residual 5.50677e-11; momentum3.58e-11 and continuity4.18e-11.
Flux error6.00e-10, max divergence1.91e-9/s and energy imbalance2.04e-10 pass.
Physical operators/full FE/load/metadata remain float64; only the separate Float
preconditioner and unchanged restart60 flexible true-residual method are used.

| Drag quantity | Uniform control |
| --- | ---: |
| Pressure |.0249631553N|
| Raw symmetric viscous |.0140018711N|
| Reaction |.0398094421N|
| Raw/reaction mismatch |2.12114%|

Versus accepted cosine count6 base, pressure/viscous/reaction changes .468%/.191%/
.420%, inlet pressure/dissipation .300%/.320%; raw gap2.171%→2.121% stays above1%.
It costs12.2% more time and27.0% more owned memory than that smaller mesh.
Versus cosine normal, changes .657%/1.190%/.00131% and raw gap2.108%→2.121% keep
physical gates open. It is11.1% faster and3.84% more memory than that same-size
mesh. Serial local costs are observations, not universal performance claims.

The unchanged read-only stress observer completes59.97s; sampled467.344MiB and
owned467.453MiB remain distinct. Inputs and original weak/raw identities pass.
Global equilibrium defect 0.13049 and jump 0.01653 improve against cosine
normal .16594/.01988 (~21.4%/~16.8%). Smaller global measures do not close force
gates or provide an error bound. Retain the conditioning/diagnostic control;
do not physically adopt or expand this family merely on global indicators.

Next [isolate X-normal refinement with actual cosine body surface held](cfd_3d_second_normal_goal.md).
Stage1, native physical certification and broad goal remain open. Evidence is
`build/c3d-force-transition/checkpoint-audit.json`, immutable geometry/source
cohorts, pressure-extraction limitation, seven support controls, count6/count8
stages, accepted field, force comparison and stress receipts. Internal local
reference targets are `make test-cfd-reference3d-force-transition` and
`make test-cfd-reference3d-uniform-normal`; public headless quickstart is unchanged.
