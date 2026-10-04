# Bounded complete vector-block velocity operator and exact shared factor

Caller-owned scalar Cholesky passes count6 base (160 / 116.2 s / 1353 MiB), but
exact normal stage predicts 1912 MiB versus unchanged 1800 cap. Prior vector
symbolic graph alone is rejected because it increases factor/workspace requirements
and keeps old representation alive. This slice must replace the full live scalar
velocity representation with complete 3x3 node blocks used by both exact physical
operator and exact caller-owned factor, so index/ordering savings are measured,
not assumed. Normal vector symbolic requirements are 1336344064 factor and
49771768 scratch bytes; their modest increases remain in the budget.

Build sorted upper node-pair blocks in bounded batches from every original scalar
upper velocity coefficient. Preserve all entries (including tiny values); pad
missing block entries with true structural zeros. Retain both component directions
and full symmetric diagonal blocks. Record/reconstruct original CSR coefficients
bitwise and independent operator action before releasing its redundant storage.
Physical coordinates remain component-major; factor permutations/interleaving
must be bijections. Share block arrays/starts with installed blockSize=3 symmetric
Cholesky. Prove row-major upper block / column-major lower transpose convention,
exact dense/anisotropic/refined action/inverse, complete mixed/pressure/RHS/full
FE/field preservation, symbolic costs and partial/100 owned cleanup. Implement
optional exact fused block action only with independent mathematical controls.

Original L4 first, useful accepted control to admitted count6 base for complete
field/force equivalence and actual cost. Measure stopped normal symbolic/live-RSS
budget plus exact factor/scratch and 32-MiB reserve; reject numeric launch unless
it fits. Accepted normal numerical result enables force refinement, not physical
certification. Preserve 50000-tet/1800-MiB/180-s/3000-iteration and full FE/flux/
divergence/energy/force/publication gates, requested target and all failures. No
shift/floor/scaling, pressure mode removal or resource relaxation. No native/shared
API/dependency/version/commit/package/install/deploy change; extraction deferred.
Nodal-vanishing approximation stays deferred. Stage 1/full goal remain open.
