# Next bounded hypothesis: distributed macro P2 correction plus cheap local factor

Preserve restart6/30 controlled-fill failures, previous global-polynomial velocity
failures and prior macro P2/P3 expensive-sweep rejections. Thirty global polynomial
velocity directions and greater Arnoldi restart do not cure remaining distributed
momentum error. Full force testing continues to use the successful complete
Cholesky/pressure-ten recipe; the eight-interval geometry remains memory-withheld.

Build a distinct balanced two-level velocity PC with the already proved exact
nested macro P2 interpolation (vertex and edge modes) and the cheap degree-ordered
controlled-fill local inverse. Replace global quadratic30, retain complete physical
P4/DG-P3 velocity/pressure action, exact boundaries/load and balanced pressure-ten
complement10. Do not replay the old expensive exact-component-sweep recipe.

First inspect/reuse cfd_reference3d_quadratic_coarse.py and its sealed tests/evidence.
Construct exact Galerkin velocity Ac=Z^T A Z in bounded sparse/local macro batches,
preferably while original local Schur matrices are already live during assembly.
Avoid an unbounded full A@Z product or Nv-by-coarse dense columns. A separate
Double coarse factor library is required: encoded Float workspace ABI is not the
old SharedTriangleFactor Double ABI. Bind each library/build/source separately.
No altered physical or coarse diagonal/equation; optional numerical symmetry
restoration only within independently checked roundoff, not a stabilizing shift.

Predeclare sparse interpolation/coarse matrix/construction memory, exact coarse
symbolic factor/scratch requirements, both actual Arnoldi bases/work and pressure
work, local factor, full currentRSS and32MiB. Fresh admission before numeric coarse
factor and local factor; immutable symbolic requirements are not RSS predictions.
Original1800MiB/180s/3000iter/50000tet/full1e-10/retained1e-11 and original
force/flux/div/energy/atomic publication checks. Owned highwater remains authority.

Independent dense balanced formula/SPD/coarse reproduction, exact local vs global
Galerkin equivalence on anisotropic FE with nonzero loads and full pressure
reconstruction, coefficient/permutation preservation, injective essential-boundary
projection, invalid/bounds and partial/owned cleanup must pass. ONE small4992tet
strict full control needs useful whole cost declared before measurement. Only useful
proof permits matched28416tet accepted cube; require >=300MiB complete new velocity
factor/work saving and meaningful runtime. Only useful large proof permits saved
finer43008tet L4 and separate original force comparison, then matching L8 and a
second true resolution step. No failed unchanged retries or cap/target relaxation.
Optional reference proof, native integration, app installation and physical
accuracy remain distinct; broader user goal stays active.
