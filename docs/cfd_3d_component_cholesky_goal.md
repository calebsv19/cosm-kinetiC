# Bounded repeated component sweeps with exact sparse Cholesky blocks

Declared after symbolic diagnostics identify 1325505336 bytes of exact coupled
factor storage on the 23616-tet normal cube, with only 26931968 bytes of numeric
workspace. Exact scalar interleaving saves merely 2.65% factor storage while
raising conversion/analysis peak; the complete vector graph increases required
storage/workspace. Neither gives robust normal-mesh headroom as implemented;
no numerical candidate or physical mesh is promoted from these diagnostics.

Revisit the prior mathematically SPD component sweep using the measured faster
installed exact sparse Cholesky backend rather than the old generic LU component
factors. The prior single sweep reached full residual 7.29e-7 and four fixed sweeps
approached the numerical gate but hit 180 seconds. Do not retest plain uncoupled
components as though they solved the coupling problem. Keep all upper off-component
couplings and both symmetric directions in block forward/backward sweeps.

Use exact positive principal-component Cholesky factors and a fixed repeated
symmetric block Gauss-Seidel/SSOR inverse. For M=(D+L)D^-1(D+U), M-A=L D^-1 L^T is
positive semidefinite, so the fixed Richardson sum with a sweep inverse remains
SPD. Preserve original mixed operator/RHS, pressure modes/diagonals/mass, all
local elimination/reconstruction, full FE residual, raw traction/reaction,
conservation, energy, resource gates and atomic publication. No floor, shift,
scaling, pressure removal, modified force or residual criterion. Explicit triangle
adapters must not pretend upper storage is a full CSR matrix.

Prove dense and anisotropic/refined exact component inverses, sweep symmetry,
linearity, positivity/majorization, repeated-sweep action, original input/equation
preservation, non-SPD rejection and owned cleanup. Record actual component factor
storage, full setup/solve/reconstruction/diagnostic time, allocations and owned/
sampled RSS. Start with the exact original cube against its accepted field under
the unchanged 50000-tet/1800-MiB/180-s/3000-iteration caps, using predeclared fixed
sweep counts. Retain iteration/time failures without fields; do not relax targets.

Only a useful accepted original control may extend to the accepted count6 base
for full field/force equivalence and measured total cost. Admit the exact stopped
normal only with demonstrated resource headroom and complete numerical gates.
If the lower-memory approximation is slower, report that tradeoff honestly and
adopt only where a completed larger control establishes practical value. Rejected
controls remain evidence. A successful solve still needs separate refinement/
domain/component/raw-reaction/stress physical qualification.

No native/shared API, dependency, version, commit, package, install, release or
deployment change. Generic FE/factor/local-job extraction stays deferred. The full
object/wind-tunnel goal and Stage 1 remain open; native traction and authored object/
transient/outlet/inertial-wake scope follows physical reference qualification.
