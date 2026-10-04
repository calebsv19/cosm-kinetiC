# Same-surface streamwise refinement: useful force evidence, physical gate open

Thirteen focused geometry, complete-FE, Float coefficient/solve/lifecycle and
matched-domain controls pass. The independently accepted28416-tet L4 second-normal
field improves the raw/reaction gap while keeping every force component change
below1%. It remains a diagnostic field, without physical mesh adoption, because
raw/reaction is still above1% and the matched longer-tunnel numerical field is
resource-withheld. Stage1/native physical qualification and the broad goal remain
open. The complete original float64 equations, pressure modes, loads, target and
resource limits are unchanged. No native/default source, commit, package or install.

## Distinct geometry and resource experiments

| Experiment | Tetrahedra | Actual result |
|---|---:|---|
| Conforming original-macro plane cut |41216|2360.30MiB estimate; numerical factor/field withheld |
| Identical Float scalar graph on that cut |41216|1832.89MiB owned high-water at symbolic setup; cap rejected |
| Non-nested same-surface L4 tensor remesh |28416|1675.78MiB stage estimate; complete solve accepted |
| Matched held-near-field L8/end2 tensor remesh |33216|1981.46MiB estimate; numerical factor/field withheld |

Caps remain1800MiB/180s/3000iterations/50000tet. Sampled scalar RSS was
1571.19MiB; it does not replace the larger owned
high-water mark that triggered rejection. Original-macro cutting proves individual
parent containment/volume, old vertices bitwise unchanged and reflected actual
cells. It needs many more tetrahedra than rebuilding tensor diagonals. Scalar
ordering omits only exact zeros and duplicate symmetric entries and preserves all
rounded nonzero coefficients; its full-cube symbolic cap failure is retained.

The separate tensor remesh explicitly crosses some old tetrahedron interiors;
tests prove this non-nesting rather than claiming original-tet containment. Its
bricks exactly partition old bricks. It preserves every original axis plane,
physical domain/BC/cube surface triangle and actual reflected cells. It halves
closest streamwise normal spacing0.09375→0.046875m. Global worst intrinsic shape
53.3446747993 and Jacobian condition215.373403800 are unchanged. Local centroid-edge
shape measures worsen; none is represented as a force error bound or reassigned
admission of an older rejected geometry. Matched L8 holds the translated23616
inner tetrahedra and actual cube triangles exactly to canonical coordinate
precision; only outer-domain geometry differs. Its Jacobian maximum remains
271.7227700732. No L8 field or domain-force convergence claim exists.

## Accepted original-equation field and force comparison

Complete solve: 120iterations /87.750311s /
1620.734375MiB owned peak /
1620.734375MiB sampled peak. Original fullFE residual
8.31615861622e-11, momentum
4.63053382224e-11, continuity
6.90772398487e-11, requested1e-10 met.
Flux error1.2924552921e-09, maximum divergence
8.42511420612e-09/s (below1e-8, with limited margin), energy
imbalance5.0450057972e-11. Both flexible bases, original
block/coupling/pressure action, immutable Float factor input and load/catalog
restoration are verified. Actual live allocation guard is retained in the receipt.

| Quantity | New value | Change versus accepted cosine count6 normal |
|---|---:|---:|
| Pressure drag |0.0248382327932N|0.152867%|
| Raw symmetric viscous drag |0.0142178104881N|0.334129%|
| Reaction drag |0.0397598793908N|0.125811%|
| Inlet pressure |0.0223474668978Pa|0.087183%|
| Dissipation |0.000178979115307W|0.087331%|

Raw/reaction discrepancy decreases
2.107967→
1.770217%; useful evidence,
but the separate1% physical gate still fails. Source-bound comparison is
`build/c3d-second-normal/force-comparison.json`. The field is retained diagnostically;
no raw force is replaced by reaction force. Larger cost is measured, not assumed
universally beneficial.

## Independent signed observation and conditioning limitations

The unchanged signed observer completes in88.343388s /
569.265625MiB owned peak. Both lift identities close within
4.26342988691e-14N and signed attribution recovers the remaining
0.703836109mN
force gap. Full per-cell pressure/viscous volume/jump arrays and cancellation are
retained in an immutable NPZ, with input receipt/result/snapshot rechecked.
Volume stress defect0.185311400389 and interior
jump0.0201135691831 exceed the prior cosine normal diagnostic
0.165943552149/0.0198825599513 despite the smaller force gap; stress norms are not
force certificates.

Retained initial arbitrary-dual-load test reaches pressure coefficients4.75e9
and loses6.66e-8 in recovered mean. Independent field-derived unit velocity control
has2.18e-9 pressure coefficient error but7.34e-14 original full residual and8.33e-17
mean error. Explicit velocity scale0.01/pressure scale1 passes unchanged1e-10
coefficient reconstruction with2.33e-11 pressure error and6.57e-14 full residual.
The complete nonzero arbitrary-load action/eliminated equations pass. Two initial
failed logs/sources and quantified controls remain immutable. No universal absolute
pressure coefficient accuracy or full DG-spectrum certificate is asserted, and
physical full residual/force gates are not relaxed.

## Evidence and next bounded gate

- Nested geometry: `build/c3d-second-normal/geometry-survey.json`.
- Tensor geometry: `build/c3d-second-normal/tensor-geometry-survey.json`.
- Frozen stages: `build/c3d-second-normal/stage-runs/` and `scalar-stage-runs/`.
- Accepted field: `build/c3d-second-normal/runs/2ee8302e601649903475709bda92af9e777ec1597315d4fefed85828e1ba6c18/L4-body6-second-normal-tensor-receipt.json`.
- Signed observer: `build/c3d-second-normal/observer-runs/569ce434adcd9e729b4af9637394ffa2a55b6ed93e2796c9cc8d3bcf60dc83d0/L4-body6-second-normal-tensor-signed-force-receipt.json`.
- Support:13controls in three primary suites plus matched-domain geometry.
- Once-only closure: `build/c3d-second-normal/checkpoint-audit.json`.

Next use a declared strength-distribution/diagonally compensated approximate
preconditioner experiment to reduce factor storage while retaining the complete
physical operator, then recover the exact matched L8 force test if measured useful.
See [unchanged gates and next sequence](cfd_3d_normal_force_next_goal.md).
The optional reference mesh helper/runner is integrated for diagnostics; physical
adoption and native/general-object behavior require later evidence.
