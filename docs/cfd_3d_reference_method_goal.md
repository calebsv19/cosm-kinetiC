# C3D stronger reference method: bounded support gate

2026-09-30, existing Main Edit; continuation of the six-stage plan in
cfd_3d_initial_improvements_checkpoint.md. Preserve all predecessor evidence and
native equations. No commit, package, install, dependency upgrade or shared change.

Evaluate continuous cubic tetrahedral velocity / discontinuous quadratic pressure
on Alfeld (one barycenter, four children per macro tetrahedron) splits. The
Guzman-Neilan result supports this pair for degree >= dimension on shape-regular
barycentric refinements: https://arxiv.org/html/1710.08044v1, sections 1 and 6.1.
This mathematical support is not qualification of our implementation or meshes.
The installed library lacks cubic tetrahedral H1 support, so implement a small
app-owned reference element with explicitly sorted vertex/edge orientation.

Predeclared sequence and stop conditions:

1. Verify nodal identity, degree-three polynomial values/gradients, global edge
   continuity, split volume/boundary preservation, and local divergence-map rank.
2. Solve independently forced polynomial Stokes, including natural vector-Laplacian
   traction, pressure datum and all flat-wall orientations. True residual and
   representable field/traction errors <= 1e-8. Divergence volume and wall traces
   must be measured rather than imposed in the observer.
3. Calibrate an empty square duct against the existing Fourier reference. Then
   run a bounded cube coarse/refined pair with raw pressure and symmetric viscous
   loads, reactions, dissipation and flow. Preserve original <=1% component
   convergence/reaction gates; no replacement force or stress projection.
4. Each numerical child: <=50000 tetrahedra, <=1800 MiB observed own RSS, <=180 s,
   <=3000 linear iterations. Declare DOFs before assembly, retain source hashes,
   commands, output and failed receipt. Stop on support/cost/residual failure; do
   not launch a broad sweep or quietly increase caps.

This slice advances stage 1 only unless its actual physical gates pass. Stage 2
native correction and stage 3 scene qualification require a reliable reference.
