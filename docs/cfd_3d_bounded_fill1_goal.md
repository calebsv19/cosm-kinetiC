# Bounded level-one fill with deterministic degree ordering

After4662e63459d5d15562d40c2ee684a36b1b5e18d7974114ff42fd6be4bb632b4f, preserve all
prior rejected PCs/meshes and accepted force-sensitivity field. Distinct velocity
PC uses ascending original symmetric node degree (node index tie break), all
original3x3 blocks and candidate fill from pairs of ORIGINAL lower neighbors at
one elimination pivot. Each new column retains at most its ORIGINAL off-diagonal
count of additional candidates, choosing smallest degree/index order. This is a
truncated level-one pattern, not complete IC1. No physical coefficient/mode drop.
New factor graph<=2*original_blocks-nodes. Extra entries initially zero; original
Float32 predictor blocks are copied directly into Double numeric factor under the
bijection (no expanded Float32 value copy). Omitted Schur pairs receive positive
Frobenius diagonal compensation; Gershgorin fallback only inside PC. Same balanced
global velocity30 and qualified pressure-ten complement10 remain, but their old
qualification does not transfer automatically to this new inverse.

Symbolic working-memory bound384MiB. Precompute candidate count and conservative
all-live native arrays/export-copy bound before allocation; reject exceeded bound.
Fresh currentRSS+known construction bound+32MiB<=1800MiB before pattern allocation.
After symbolic/physical-input proof, fresh factor stage includes72*pattern_blocks+
1024handle bytes,32*nv solve scratch, both Arnoldi bases/work, complete pressure and
velocity coarse reserves, actualRSS and32MiB. Owned highwater never reset; Cphysical
prefix byte-identical; full Float64 P4/DG-P3/degree6 action/reconstruction authority.
Same1800MiB/180s/3000iter/50000tet/full1e-10/retained1e-11/flux/div1e-8/energy.03 and
original force/atomic publication rules. Graph diagnostic is not convergence proof.

Independent support: Python oracle for degree permutation and truncated level-one
graph (all original edges survive and cap exact), full-pattern dense factor/coordinate
identity, sparse SPD factor error positivity and bounded inverse, balanced coarse
formula/reproduction/complement, anisotropic FE/nonzero load/full pressure/action,
invalid bounds/permutations/nonfinite/lifecycle/partial cleanup and reservations.
One original4992tet full control only after support and symbolic bounds pass; all
full equation/output identity gates and whole<=2*11.750901166s. A useful small
control permits one matched28416tet L4: whole<=1.25*85.313668042s, factor plus all new
velocity work>=300MiB smaller than873198472byte old factor, exact physical identity/
separate force and scalar equivalence. Only useful large proof permits one saved
quality-passed43008tet finer L4 under fresh admission. No unchanged failed retries,
cap/target relaxation/default/native/package/install adoption. Broader goal active.
