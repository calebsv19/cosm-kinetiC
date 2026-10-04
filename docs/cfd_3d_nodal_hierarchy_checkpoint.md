# Nodal-vanishing basis is correct; measured preconditioner is rejected

Six support controls prove sparseTZ=I,TY=0, exact triangular coordinate change
[Z,Y]=[Z,E][I,-TE;0,I] and bijection from existing vertex/edge/face pivots.
Dense/anisotropic/refined controls verify original affine interpolation and boundary
constraints, exact congruent coupled blocks, fixed symmetric SGS/positivity/
majorization/linearity, original nonzeroFE/RHS/pressure/reconstruction and partial/
100 factor cleanup. Original physical equations, load and pressure modes remain
intact; nodal coordinate norms are nonorthogonal diagnostics, not full residual.

OriginalL4 single sweep numerically passes the existing full gates at2861iterations,
81.4141s /604.359MiB owned peak (sampled602.984MiB). Independent originalFE
residual7.94042e-10 is dominated by momentum7.94042e-10; continuity5.66466e-13.
It misses requested1e-10. MINRES returns info0 before that true target is reached;
this is not silently treated as requested-target success. Complete numerical
field publication under the existing1e-8 linear gate remains visible.

Field equivalence against accepted exact catalog anchor: maximum velocity change
1.9713e-12m/s and pressure3.5160e-9Pa, component/scalar relative changes<1.2e-10.
Full mixed velocity/coupling/pressure matrix hashes, mesh/free/RHS identities match.
Raw/reaction3.644% on this coarse original still fails physical qualification.

Exact unshifted coarse/fine factor bytes51942496+42383176=94325672; maps satisfy
left inverse4.44e-16 and nodal vanishing3.18e-16. These smaller factor bytes do
not imply a cheaper whole solve. The exact catalog anchor takes16.061s /385.609MiB:
new candidate is~5.07x slower and~56.7% more memory. Previous unit split failed
full residual3.55e-4 at3000; the new basis improves that convergence but fails
requested target/useful cost. Do not adopt or extend to larger meshes.

Retain all receipts, accepted equivalent snapshot, target miss, costs and prior
sealed evidence. All1800MiB/180s/3000iteration/50000tet and physical gates remain.
Native/shared API/version/package/install/commit are unchanged; Stage1 and broad
object/wind-tunnel goal remain open.

The next bounded route tests lower-precision storage only for the coupled velocity
preconditioner, retaining every original float64 physical coefficient/action/load,
pressure mode, reconstruction and independent full FE residual. Installed SDK
exposes float factor/scratch/solve APIs. Float arithmetic is not an exact linear
SPD inverse in float64; a flexible outer iteration must handle that honestly.
No pressure shift/diagonal floor/mode removal or resource/target relaxation.
See docs/cfd_3d_mixed_precision_goal.md for control/ownership/admission gates.

Evidence: build/c3d-nodal-hierarchy/checkpoint-audit.json, support-test-receipt.json,
frozen runs and original-L4-nodal-single snapshot. Preconditions are in
cfd_3d_nodal_hierarchy_execution_goal.md; end-slab geometry/budget failure remains
at build/c3d-end-slab/checkpoint-audit.json.
