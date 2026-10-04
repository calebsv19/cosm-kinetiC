# Fixed L4 cube: balanced tensor body-resolution survey

Fresh bounded goal after calibrated empty-duct checkpoint d1300537f12848ee20f155ba42bad82c147dc403626c0374c04a7eaeb1e3a254.
Preserve all preceding rejected geometries, complete sources/fields, original cube
physical gates and native hashes. Baseline is the accepted28416tet L4 second-normal
cube; no claim that original tetrahedron partition or surface triangles are retained.
Physical cube[1.5,2.5]x[.5,1.5]^2, L4x2x2 box,15m3fluid and body6m2 remain exact.

Declare eight geometry-only candidates before seeing results: cosine8 and cosine7
plus explicit midplane (eight actual body intervals), each with streamwise relocate
or retain closest pre-second-normal plane, and lateral outside distance.0625 or
.046875m. All use streamwise closest normal.046875m and split each far streamwise
slab[0,1.3125] into two.65625m slabs. Reflect the same octant across all three
midplanes and preserve y/z exchange. Surface grid is finer than old cosine6;
triangles may change. Macro mesh is mirrored tensor and re-Alfeld split. This is a
nonnested remesh, not a partition of accepted old macros/tetrahedra.

Predeclared acceptance: exact positive volume, correct boundary planes/areas,
reflection/y-z tetrahedron symmetry and <=50000tet. Global intrinsic worst shape
and Jacobian max condition must not worsen (relative allowance1e-8). Within
centroid edge bands.025,.05,.1m and body-distance bands.05,.1,.2m, require worst
intrinsic shape, volume-weighted mean shape and max Jacobian condition all nonworse
than the same baseline bands (relative allowance1e-8); edge.05 volume-weighted
mean must improve at least1%. Centroid bands differ between meshes: these are
prospective geometric controls, not matched-parent or force-error bounds. Maximum
cube surface triangle edge must decrease and body triangle count increase.

Each candidate is evaluated and retains reasons. Only a passed candidate may be
selected: lowest edge.05 weighted shape, then tetrahedron count, then declared name.
No candidate mutation or relaxed quality gate after results. Reject all failures
before symbolic factor or numerical solve. Geometry supervisor caps180s/1800MiB;
no physical accuracy, numerical field or force acceptance inferred from geometry.
Independent controls must prove intrinsic shape invariance, volume/boundary and
reflection correctness, midplane topology and invalid parameter refusal.

If geometry passes, only selected geometry proceeds to original strict mixed
reference stage and fresh numeric admission: P4/DG-P3, exact condensation, complete
float64 equations/load/pressure, Float coupled PC, flexible6 with retained1e-11 and
requested full1e-10, both bases/work +factor/scratch+32MiB reserve, unchanged
1800MiB/180s/3000iterations/50000tet, flux/div1e-8, energy.03. Fixed-domain compare
separate pressure/rawviscous/reaction force, raw/reaction mismatch, original Pin/D
and original combined1% physical gate. Empty-duct excess remains additional
separate diagnostic; no force substitution or native/default/package change.

If all eight geometries fail, seal results and move to a distinct exact accepted
mesh pressure-preconditioner/storage investigation; do not repeatedly retry failed
geometry with weaker quality. Broad Stage1/native/general qualification stays open.
