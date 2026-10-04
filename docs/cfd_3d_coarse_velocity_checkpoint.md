# Macro-linear balanced velocity correction checkpoint

Five support tests prove exact constant/affine reproduction, shared trace and
vertex identity, projected injectivity/essential boundary exclusion, exact
Galerkin inverse, dense balanced formula, symmetry/linearity/positivity, exact
coarse action, anisotropic FE/RHS/pressure reconstruction, partial and 100 lifecycle
cleanup. An initial support-only sparse LinearOperator comparison is invalid;
the retained log records it, and an independent dense Galerkin comparison fixes
the test. No frozen numerical source or predecessor is altered.

Original L4: 444 coarse velocity unknowns, 224632 coarse factor bytes;
1852 iterations, 65.2565 seconds, 496.297 MiB owned peak, full residual
4.13269e-10. All unchanged numerical/publication gates pass; requested 1e-10
full-residual target is missed. Field/force equivalence to the exact coupled
factor passes. Raw/reaction mismatch remains 3.64358%, physically unqualified.

Against the exact two-block one-sweep control, iterations fall only 8.63%, wall
time rises 24.06%. Reject extension to the larger base: insufficient measured
improvement on the original. This is a retained accepted diagnostic field, not
an adopted stronger path. Evidence is `build/c3d-coarse-velocity/checkpoint-audit.json`
and frozen `original-L4-coarse-single` receipt/field, five final support proofs.

The next [macro-quadratic coarse-space control](cfd_3d_quadratic_coarse_goal.md)
adds edge modes to the same fixed balanced SPD formula. Full equations and pressure
modes remain intact. Physical accuracy, Stage 1 and the full goal remain open.
No native/shared API/version/commit/package/install/deploy change.
