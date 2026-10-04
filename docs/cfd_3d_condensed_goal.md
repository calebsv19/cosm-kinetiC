# Exact macro condensation toward finer cube force resolution

2026-10-03. Continue the active object/wind-tunnel goal from the quartic stress/
mesh checkpoint. Eliminate local bubble velocity and nonconstant DG pressure
unknowns exactly, retaining trace velocity and one pressure coefficient per macro,
then reconstruct the full original P4/DG-P3 field. No pressure/stress smoothing,
penalty, regularization, equation change or physical acceptance relaxation.

Predeclare algebra controls against explicit uncondensed matrix actions including
nonzero loads and nonzero prescribed velocity, pressure gauge and natural ends.
Recover known quartic velocity/cubic pressure solutions across axes/meshes/pressure
offsets and flat-wall raw stress. Independently evaluate full residuals with the
original finite-element quadrature after reconstruction. A condensed residual is
only solve progress, never full residual authority. Verify original cube mesh,
RHS/free-DOF identity, full field and raw force/reaction/energy equivalence before
claiming measured memory savings. Keep the inherited complete numerical acceptance
and atomic resource-checked field publication. Frozen prior fields/sources/workers
and audits remain intact; no native change or shared API change is implied.

Use any proven resource savings to resume finer cube stress-resolution controls,
including previously memory-stopped normal pairs, within unchanged 50000-tet,
1800-MiB, 180-s, 3000-iteration, true residual <1e-8, divergence/flux <1e-8 gates.
Raw pressure/viscous/reaction/scalar convergence and raw/reaction mismatch must
still be <=1% on L4 and L8. Reference qualification precedes native correction,
authored physical objects and transient/outlet/inertial wind-tunnel validation.
A useful memory/solver improvement alone does not complete stage 1 or the full
goal. Generic FE/core_jobs extraction remains deferred; app-owned reference and
local-supervision policy reuses existing element, quadrature, traction, energy,
scene and numerical acceptance semantics. No commit/package/install is implied.
