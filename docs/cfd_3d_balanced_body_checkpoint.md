# Eight balanced body grids rejected before factorization

Four independent support tests pass after two pre-survey corrections: exact cosine8
midpoint representation and independently derived expected tetrahedron count.
The initial failed support log is preserved. The bounded geometry survey completes
6.304s with sampled279MiB, rebuilds the accepted28416tet L4 mesh bitwise, and
screens all eight predeclared candidates. No candidate passes; no factor or field.

Cosine8 and cosine7-plus-midplane grids have eight actual body intervals,768cube
surface triangles versus432, with maximum triangle edge.2706/.2835m versus.3536m.
Relocate candidates have43008tet, retain candidates49920tet. Every candidate
preserves cube/domain, positive15m3volume, boundary areas, three reflection
symmetries and y/z exchange. All are explicitly nonnested remeshes, with changed
cube triangles and no old macro/tetrahedron partition claim.

| Family | Lateral distance m | Edge.05 weighted shape ratio | Rejection |
|---|---:|---:|---|
| cosine8 relocate/retain |.0625|.8701|edge.025 worst shape; body.2 worst shape/condition; retain also mean |
| cosine8 relocate/retain |.046875|1.0042|edge.025 worst/condition; edge.05 mean; body.2 worst/mean/condition |
| cosine7mid relocate |.0625|.8246|body.2 worst shape and condition |
| cosine7mid retain |.0625|.8246|body.2 worst shape/mean/condition |
| cosine7mid relocate/retain |.046875|.9225|edge.025 condition; body.2 worst/mean/condition |

Several improve global worst shape/condition and mean near-edge shape, yet violate
prospective local transition-region gates. No global or average improvement overrides
a local failure. Centroid regions differ between remeshes and are geometric controls,
not force error bounds. All eight are rejected before symbolic/numeric factor.
The original accepted fields and1.757%raw/reaction physical gap remain unchanged.

Evidence build/c3d-balanced-body/survey-runs/49c2545966fe29771b526266a80b3ab39eb0a3a8f14a6203625fbd447d312c41,
support-test-receipt.json and checkpoint-audit.json. Continue a distinct pressure
preconditioner investigation on the accepted original meshes, preserving all full
modes/equations/targets/resource limits. Broad Stage1/native/general qualification
remains active. No commit, package or installation.
