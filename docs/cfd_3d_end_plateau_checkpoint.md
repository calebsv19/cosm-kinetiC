# End refinement resource stop and signed force-local mesh evidence

The three-layer held-inner L8 count6 normal cube has33216tetrahedra. Two physical
mesh controls prove the same18816inner tetrahedra and body triangles up to original
numbering and1e-13m coordinate rounding, fixed cross section/flow geometry,
positive mapping, volume31m3, genuine physical boundaries and improved end shape.

Its unchanged Float+restart60 symbolic/current-residency estimate is
2063587685bytes (1967.991MiB), above1800MiB. This includes1125998956factor bytes,
38745361numeric scratch,624656384current RSS,240632552flexible basis/work reserve
and33554432reserve. Symbolic handles clean up, original complete mixed operator/
pressure/load/action inputs remain preserved, and no numeric factor or field is
created. The diagnostic completes21.77s; sampled1165.125MiB is not a numerical
factor allocation estimate. Original caps and every prior accepted field remain.
The end2→end3 force plateau is therefore not established.

## Adopted diagnostic: signed cell and face force residuals

A separate observer retains all original pressure/viscous weighted element stress
divergence and interior jump terms for the .25m/.4m lifts and all three axes.
Half each interior jump is allocated to each neighboring cell only for diagnostic
ranking, with original separate face/volume arrays retained in immutable NPZ.
Pressure/viscous cancellation and centroid bands remain explicit; this is not a
clipped-band identity, force replacement or proven force-error bound.

Four support controls pass against the original observer: arbitrary discontinuous
pressure and non-solenoidal velocity, smooth quartic/cubic fields, pressure-only
facet quadrature8/10 and signed allocation/cancellation/refusals. Every original
stress result is unchanged. Three actual-cube controls independently group signed
loads by actual parent barycenter IDs, prove symmetric pairs and physical geometry
refusal, and reject unbound/tampered diagnostics. The initial relocated-fixture
failure lacked its frozen source directory; the completed fixture reaches the
intended path-binding refusal. That failure and first survey are retained.

The accepted28416tet end2 field observation completes96.77s/484.172MiB. Original
full residual/flux/divergence/energy and immutable field checks pass; the previous
stress rows and indicators match exactly. Maximum signed identity error is about
3.03e-14N. Both lifts recover the same0.839245mN total raw-minus-weak discrepancy,
consistent with the original raw-minus-reaction gap.

| Lift shell | Pressure signed drag gap | Viscous signed drag gap | Total |
| --- | ---: | ---: | ---: |
| .25m | -5.632448mN | +4.793203mN | -.839245mN |
| .40m | -11.721333mN | +10.882088mN | -.839245mN |

Individual component splits depend on the lift and cannot serve as physical force
references. Absolute cell-total sums7.589/10.558mN exceed the net gap, revealing
cancellation. Highest-scored cells lie roughly.036m from cube edges. With half-face
allocation, below.025m net contributions are -.511/-.504mN for the two lifts;
.025-.10m and .10-.40m regions contain additional positive and negative terms.
Do not collapse this distribution into a single positive global indicator.

## Sixteen prospective local candidates rejected before algebra

Signed pressure+viscous Alfeld-child drag is summed per macro before magnitude,
then ranked by the maximum across both lifts and eight mirror-orbit magnitudes.
The first eight Y/Z-paired sets within.15m centroid edge distance are screened
with conforming bisection and all-edge-star methods. Positive mapping, volume,
mirror symmetry, true boundaries and body/inlet/outlet/wall areas6/4/4/64m2 pass.

All16fail the affected intrinsic worst-shape gate. The top paired bisection adds
384tetrahedra (28800total), captures4.434% of diagnostic macro score and raises
affected worst shape5.324→7.822. Its mean slightly improves4.453→4.418; that does
not justify accepting worse transition cells. The corresponding edge-star mesh
has30400tet and raises affected worst shape6.287→15.225. No candidate reaches
factor admission or generates new forces. These are geometry results, not failed
PDE fields or certified physical refinements. The strengthened requested-path
binding repeats the same16results exactly; both source cohorts remain frozen.

The new signed observer and prospective resource/quality diagnostics are useful
reference tools. No new physical mesh is adopted. End-mesh convergence and the
2.104% raw/reaction gap remain open. Stage1, native physical certification and the
broader useful object/wind-tunnel CFD goal remain active; no native/API/version/
commit/package/install change occurs in this slice.

The next step is the [bounded transition-cell quality investigation](cfd_3d_force_transition_quality_goal.md),
followed by unchanged fullFE/resource/force gates on any admitted candidate.
Evidence: `build/c3d-end-plateau/checkpoint-audit.json`, symbolic stage receipt,
immutable signed observer/NPZ, both geometry surveys, nine support controls and
frozen source cohorts. `make test-cfd-reference3d-signed-force` runs the four
self-contained signed controls; `make test-cfd-reference3d-force-local` uses this
local accepted observation for three binding/geometry controls. These reference
commands do not replace the public source-checkout headless quickstart.
