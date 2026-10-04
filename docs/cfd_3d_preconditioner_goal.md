# Bounded preconditioner continuation toward force-convergence readiness

2026-10-01, existing Main Edit. User goal: diagnose and solve the exact failed
L4 cube so force-convergence testing can resume. Keep prior numeric source and
receipts intact. New experiment source and evidence live in their own lane.

Keep the Alfeld P3/DG-P2 vector-Laplacian Stokes equations, natural boundary
conditions, mesh, mu=.1 and Q=.008 unchanged. Compare matrix/RHS/free-DOF/mesh
identities across candidates. Final true residual <1e-8, early target <=1e-10;
unchanged caps: 50000 tetrahedra, 1800 MiB observed own RSS, 180 seconds,
3000 linear iterations per numerical child. Shorter diagnostic runs may stop
below that hard cap; their fields are explicitly unaccepted.

Sequence:
1. Reproduce separate momentum/continuity residuals and continuum divergence;
   measure the global macro-constant pressure subspace and mesh conditioning.
   Restricted modal checks do not certify full pressure-space rank.
2. Compare an exact scalar velocity factorization versus predecessor AMG with
   the same DG pressure mass inverse. If needed investigate a measured stronger
   pressure/block preconditioner, preserving the physical operator and caps.
3. Verify small-system symmetry/positivity/operator identity and known-answer
   compatibility. Recheck empty calibration and tighter residual stability.
4. Require the exact cube to converge with finite fields, controlled measured
   divergence/flux/physical energy, and no imposed boundary-stress replacement.
5. Resume force testing with one genuine normal-subdivision pair. Readiness is
   a numerical milestone; original raw-component convergence/reaction <=1%
   physical gates remain independent. Retain failed controls and stop any single
   child at its cap. No broad mesh/penalty sweep, native change, commit or release.
