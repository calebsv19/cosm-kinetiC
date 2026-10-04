# Bounded nested macro-cubic coarse velocity investigation

Macro-quadratic balanced correction improves original to 1046 iterations/48.2 s,
but exact count6 base stops at 180 s / 1100 iterations, momentum 1.04e-7,
continuity 5.96e-10, 1225 MiB. It cannot admit the exact stopped normal cube.
The next coarse space adds macro edge/face cubic modes while reducing smoothing
factor cost using one fixed exact component SGS sweep; compare two coupled blocks
on the original only if the component variant is insufficient.

Construct original sorted affine macro tetrahedra and continuous P3 nodal shape
functions, interpolated at original P4 trace nodes. Coarse cubic nodes need not
coincide with quartic trace nodes: prove a sparse P4 trace evaluation left inverse
at coarse P3 nodes, T Z=I before and after essential projection. Prove shared
edge/face orientation, constant through cubic polynomial reproduction, exact
essential-boundary vanishing and projected injectivity on anisotropic/refined
meshes. No physical DOF/mode/equation is removed. Exact Galerkin Z^T A Z remains
positive and uses unshifted installed Cholesky. Balanced SPD formula and original
FE/RHS/pressure/force/conservation/energy authority remain unchanged.

Prove dense/Galerkin symmetry, linearity, positivity, exact coarse action and
owned partial/100 cleanup. Record all coarse/local factor bytes, setup, solve,
reconstruction/diagnostic/publication time, owned/sampled RSS. Original L4 one
fixed component sweep first; only useful accepted original controls extend to
admitted count6 base. Admit exact stopped normal only with measured headroom;
if cubic coarse factor growth threatens caps, symbolic cost diagnostic precedes
any normal numeric factor. Existing chunk sizes may be compared explicitly.

Retain all failures under 50000 tets / 1800 MiB / 180 s / 3000 iterations. No
shift/floor/scaling, modified residual/force or relaxed resources. Report requested
target misses. Normal admission enables force convergence measurement, not physical
certification. Stage 1 and full object/wind-tunnel goal remain open. No native/shared
API/dependency/version/commit/package/install/deploy change; extraction deferred.
