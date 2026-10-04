# Bounded coupled preconditioner precision control

Exact float64 coupled factor converges accepted original/base/normal L4/L8 but
held-normal L8 end-slab refinement needs2254MiB. Nodal/component/coarse variants
are too costly or stall momentum. Test float32 storage/factor only for the coupled
velocity preconditioner. Preserve the original complete float64 physical mixed
operator, every coefficient/mode/load and full reconstructedFE residual. Float32
preconditioner arithmetic is approximate and not exactly linear in float64;
use a bounded flexible right-preconditioned outer Arnoldi iteration, not an
unsupported claim of exact SPD inverse/MINRES equivalence.

Read/freeze installed Solve.h float symbolic factorSize_Float/workspaceSize_Float,
caller-owned16-byte factor/scratch lifetime and float solve APIs. Freeze strictC11
compiler/library/source receipts. No private opaque-factor inspection. Own separate
rounded preconditioner values; hash original float64 inputs/action before/after.
Reject nonfinite/overflow/zero-rounded nonzero values and failed positive Cholesky
without shifts/floors. Keep symbolic/current-RSS exact reservation plus32MiB,
repeat live numerical admission, record input-copy and flexible basis storage.

Prove dense/anisotropic/refined physical operator/pressure/nonzeroFE/RHS/
reconstruction preservation, approximate inverse accuracy/finite/repeat/ownership
controls (precision-specific diagnostics do not weaken physical residual gates),
partial/nonpositive/100 cleanup. Prove flexible outer iteration on known indefinite
mixed matrices with all modes, varying approximate actions and independent float64
true residual. No convergence inferred from projected residual alone. Original
cube first; useful complete target/cost result only to admitted count6 base/normal,
then exact held-normal L8 outer2 stage and numerical guarded attempt.

Preserve requested1e-10 full residual, complete numerical/force/conservation/
publication gates and50000tet/1800MiB/180s/3000outer iteration caps. Choose a fixed
restart bound in advance; record actual basis allocations. No cap/tolerance retry,
accepted-target misses hidden, force replacement or premature physical/native
certificate. Native/shared API/dependency/version/package/install/commit remain
separate. Stage1 and broad goal remain open.
