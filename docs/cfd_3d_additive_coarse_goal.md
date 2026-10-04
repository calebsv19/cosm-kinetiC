# Bounded additive exact cubic coarse/local velocity inverse

Balanced cubic plus two-block smoothing reaches original in 696 iterations/40.7 s,
but exact count6 base stops at 180 seconds, momentum 1.97e-8 at iteration 800,
1466 MiB. Exact symbolic coarse/principal factor storage is verified at 755082032
bytes. Full velocity applications in each balanced action add recurring cost.

Compare the fixed additive SPD inverse B=S+Z(Z^T A Z)^-1 Z^T, using the same exact
cubic interpolation/Galerkin factor and exact coupled principal sweep. The local
S is SPD; the coarse term is PSD, so their sum is SPD. This deliberately changes
only the preconditioner; it does not claim balanced exact coarse action B A Z=Z.
Keep every physical mixed coefficient/RHS/pressure mode, same local elimination/
reconstruction, full FE/force/conservation/energy/resource/publication authority.
No shift/floor/scaling or relaxed residual/resource cap.

Prove dense additive formula, symmetry/linearity/positivity, exact coarse factor,
nested interpolation/left inverse/boundary constraints, original FE/RHS/pressure/
reconstruction preservation and owned cleanup. Original L4 one fixed two-block
sweep first; useful accepted controls may extend to admitted count6 base. Admit
exact stopped 23616-tet normal only with demonstrated cost/memory headroom;
if factor growth threatens the unchanged caps, symbolic cost precedes numeric
factorization. Explicit existing chunk sizes may be compared.

Record actual factor bytes, complete time and RSS, full block residual and target
misses. Retain all failures under 50000 tets / 1800 MiB / 180 s / 3000 iterations.
An accepted normal permits force refinement testing, not physical certification.
Stage 1 and full object/wind-tunnel goal remain open. No native/shared API/dependency/
version/commit/package/install/deploy change; generic extraction deferred.
