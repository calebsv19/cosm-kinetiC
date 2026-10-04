# Tighter fill compensation: safe model, negligible measured improvement

Four support tests pass, including independent SVD/omitted-pair PSD checks over
extreme finite scales, reconstructed factor inverse, unchanged graph/storage,
actual anisotropic FE action, fixed pressure symmetry and owner retirement.
The original encoded operator and complete-factor implementation remain byte
identical. Only discarded-fill compensation in the experimental local factor
uses min(Frobenius, sqrt(one-norm times infinity-norm)), with scaled arithmetic
and a conservative roundoff allowance. No additional heap storage is used.

One frozen original 4,992-tetrahedron diagnostic completed in 12.1733 seconds,
with 853.0 MiB sampled peak RSS. Physical mesh, complete mixed matrix, RHS, free
DOFs and Float predictor match the qualified control. The same graph has 554,407
blocks and 39,917,400 factor bytes, zero shifted pivots, and positive pressure
work. All diagnostic factor owners retire; no full flow field is published.

Total compensation changes from 134227.594387 to 134226.468541, a reduction of
0.00084%. Fixed-proxy Schur-column error ratio is 0.999997527; projected coarse
error ratio is 0.999997412. Both miss the prospective <=0.90 eligibility gate.
The result is retained as a usefulness rejection before full-flow testing.
No larger/finer trial, native adoption or physical-force acceptance is inferred.

The next distinct local-factor investigation should select retained fill by
numerical importance at the same storage cap, rather than tighten an already
nearly rank-one block bound. It needs an independent selection oracle, preserved
original graph edges, coordinate/input proof, PSD omitted-pair handling and fresh
symbolic/numeric admission before a paired diagnostic and any full field.
The persistent CFD goal remains active.
