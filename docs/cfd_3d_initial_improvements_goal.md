# Initial 3D CFD reliability and traction diagnostics

Declared 2026-09-30 after the independent audit, before new solve results.
Existing Main Edit only. Preserve historical C3D evidence and native equations.

## Deliverables and acceptance

1. Reject nonfinite native cube fluid/flow input, including direct C callers.
   Validate finite scaled candidates and actual physical divergence before any
   accepted field or observation changes. Regression: invalid input, rejected
   scaling preserves accepted fields/diagnostics, ordinary solve, cap/cleanup,
   cancellation, gauge, sanitizer and one actual agent lifecycle.
2. Add explicit force reconstruction metadata to agent snapshots, distinguishing
   imposed zero normal wall derivative from the non-solenoidal energy interpolant.
3. Solve an exactly representable quadratic tangential velocity/affine pressure
   Stokes fixture on anisotropic tetrahedra, all three normal axes and two grids.
   Independent forcing and boundary data, velocity/pressure/traction errors <1e-8,
   true residual <1e-8, pressure-datum covariance and divergence checks.
4. Add independent reference diagnostics for raw normal traction, face divergence,
   vector-Laplacian and symmetric-stress volume lift loads, and their divergence
   identity. Prove identities on analytic fields before interpreting cube data.
   Diagnostic identity errors <1e-8 on nonzero scale; no load replaces the original
   physical force gates.
5. Run at most four unchanged gamma0 cube systems: existing L4 matched12 and L8
   directional12, each with/without first X-normal interval subdivision. Freeze
   sources; retain velocity/pressure snapshots, hash receipts and diagnostics in
   build/c3d-initial-improvements, separate from old audits. Require same-mesh
   predecessor force/Pin/D equivalence <1e-7. Keep each solve <=50000 tetrahedra,
   1800 MiB own RSS, 180 s, 3000 iterations. Failed jobs remain retained.

## Ownership and stopping boundary

Reuse-adopted: cfd_memory, existing mixed solve/checkpoints and scene/session
contracts; independent reference uses the installed reference venv and existing
scikit-fem basis/facet interfaces. Reuse-deferred: core_math stress/FE extraction
and core_jobs scheduling; CFD equations, acceptance and local process caps are
PhysicsSim policy. No shared API/version/adoption changes.

Stop with a saved assessment and revised next gates after these controls. No mesh
grading/penalty sweeps, new native discretization, general geometry, motion,
transport/outlet extension, commit, package, install or canonical adoption.
The cube's original 1% reference/5% native component gates remain unchanged.
