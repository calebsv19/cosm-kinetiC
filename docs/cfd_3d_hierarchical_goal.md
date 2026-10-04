# Bounded exact cubic/quartic velocity block preconditioner

Exact cubic coarse plus component/local smoothing improves global modes, but
balanced base stops at 800 iterations / momentum 1.97e-8 and additive base stops
at 1300 / momentum 2.18e-9 under 180 s. Neither produces an accepted refined field.
Test exact coupled fine-mode factors rather than repeating component smoothing.

Use an invertible hierarchy P=[Z,Y]: the existing nested macro-cubic interpolation
Z and unit quartic trace carriers Y complementary to explicit cubic pivot rows.
Choose macro vertex pivots, the quarter/three-quarter edge pivots for cubic edge
nodes, and one fixed quartic face pivot for each cubic face node. Prove the cubic
pivot submatrix is invertible by vertex/edge/face block structure and a sparse
left inverse, including essential projection/shared orientation. All quartic
physical DOFs remain represented; original velocity coordinates/operator remain
unchanged for the solve and full FE authority. No tiny coefficient or pressure
mode is dropped. Coordinate changes apply only inside the preconditioner.

Exactly project A into coarse/fine principal blocks and both off-block couplings.
Factor unshifted positive coarse and fine principal matrices with Cholesky and
use fixed symmetric block SGS; return P B_h P^T x to physical coordinates. Prove
congruence, bijective coordinate reconstruction, all mixed/RHS/pressure inputs,
symmetry/linearity/positivity/majorization, factor action and partial/owned cleanup
on dense and anisotropic/refined FE fixtures. Observe coarse/fine momentum
estimates honestly as coordinate diagnostics, with original full residual as
authority. No pressure/force/residual/resource criterion is changed.

Original L4 one fixed sweep first; useful accepted original may extend to admitted
count6 base. Only measured headroom permits the exact stopped 23616-tet normal;
if factor growth threatens caps, symbolic coarse/fine costs precede numeric setup.
Use existing chunk sizes explicitly. Preserve all failures and 50000-tet/1800-MiB/
180-s/3000-iteration caps; no shift/floor/scaling or target relaxation. Full FE/
flux/divergence/energy/resource/atomic publication gates remain mandatory.
Normal numerical admission enables force convergence testing, not physical
certification. Stage 1/full object-wind-tunnel goal remain open. No native/shared
API/dependency/version/commit/package/install/deploy change; extraction deferred.
