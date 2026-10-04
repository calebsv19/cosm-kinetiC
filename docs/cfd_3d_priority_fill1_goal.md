# Same-cap importance-ranked local fill investigation

The predecessor tighter bound was safe but did not materially improve accuracy.
This new recipe changes only the choice of retained level-one local fill. Keep
all original edges, the same degree ordering and per-column quota original_count-1,
the global cap 2*original_blocks-nodes, original Frobenius omitted-pair PSD
compensation, pivot safeguards and original Float predictor. The entire previous
C implementation remains a byte-exact prefix; append only the new graph builder.

For original edges, set w_ij=||A_ij||F/sqrt(||D_i||F ||D_j||F), using the existing
Float predictor. For each candidate i,j sum w_ik*w_jk over original lower neighbors
k only. Keep the largest scores, then ascending row index for ties. Preserve and
sort every physical edge; duplicate candidate paths aggregate deterministically.
This ranking is a preconditioner heuristic, not a physical coefficient change or
spectral certificate. Native scratch is bounded explicitly before allocation,
including weighted original/candidate arrays, counts, diagonal weights, cursors,
output pattern and padding. Both actual outer V/Z bases, pressure, distributed
velocity and diagnostic work plus 32 MiB reserve are included in fresh symbolic
admission; both factor numerics retain the unchanged fresh live-RSS admission.

Independently validate the full candidate graph with a Python oracle, original
edge preservation, quotas, duplicates, ties, permutations and predictor hashes.
Test sparse reconstructed factor/inverse/positive error model, actual anisotropic
FE and fixed pressure actions, owner retirement, invalid input and budget refusal.
Freeze support and exact source transforms before one original 4,992-tet diagnostic.

Compare against the sealed original Double pressure-column control. Require BOTH
fixed-proxy Schur W and projected K errors <=0.90 of the old values and every CG8
velocity-column relative solution error <=1.05 of old, alongside original physical
identity, pressure positivity/symmetry/reproduction and all resource/retirement
gates. These are eligibility gates only. No selection weights, gates, inner cap,
pressure count or physical stopping targets change after measurement.

Only an eligible diagnostic permits one full original small cube: full FE1e-10,
retained1e-11, original force/Pin/dissipation equivalence1e-7, flux/divergence1e-8,
physical energy .03, whole <=23.5018023327 s, unchanged 1800 MiB / 180 s / 3000
iterations / 50000 tetrahedra and atomic field publication. Then require matched
28,416-tet L4 whole <=106.642085 s and >=300 MiB complete velocity factor/work
saving versus873198472 bytes before one saved quality-passed finer L4 field.
No native/default/physical-force certification follows from a diagnostic or a
single strict field. Source remains uncommitted; no package/deployment changes.
Reuse is deferred: this graph heuristic belongs to the measured reference factor,
while original shared/core semantics and installed product stay unchanged.
The persistent CFD goal remains active.
