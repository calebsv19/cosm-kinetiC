# Exact-reference force convergence testing can resume

Fixed complementary pressure mass inverse scale10 qualifies the optional exact
33216tet L8 cube recipe. It changes only the complementary inverse in balanced
ten-direction pressure PC; original coarse correction, Float velocity factor,
Float64 physical matrix/action, all P4/DG-P3 modes, quadrature, residual metrics,
reconstruction, force/energy gates and resource caps are preserved.

Four independent support tests pass. Small4992tet original L4 completes114iter/
11.751s/owned359.906MiB with8.074e-12 full reconstructed FE residual, versus200iter
previously. One exact cube completes168iter/114.126s/1531.141MiB owned/sample peak,
full1.019967e-11, momentum8.676e-12, continuity5.363e-12. Retained1e-11 gate and
original full1e-10 gate both pass. Flux4.055e-14, maximum divergence1.452e-10/s,
energy imbalance4.957e-12 and atomic publication pass. Fresh admission and both
Arnoldi bases/work/coarse/factor/scratch/currentRSS/32MiB reserve are verified;
completed factor/coarse/storage owners release before bitwise FE/load restoration.

Versus accepted mass426iter/160.383s:60.563percent fewer iterations,59.702percent
less setup+solve and28.841percent less whole time. Compared with the earlier ten
field390iter/156.923s, original no-regression limits also pass; owned peak increases
only.708percent versus that field and remains under1800MiB. This qualifies the
exact reference recipe under declared gates; one case does not establish a general
performance policy. No native/default/desktop/package/installed adoption.

Mesh/matrix/RHS and Float predictor hashes match both older fields. Separate
pressure/raw viscous/reaction forces, inlet pressure and dissipation differ by
less than2.1e-10 relative. The raw surface/reaction mismatch remains1.756746365percent,
above original1percent gate. Physical force accuracy is NOT certified and no gate,
force definition or energy reference was replaced. Earlier rejected cubic20,
velocity refinement, general residency and geometry candidates remain rejected.

Evidence: build/c3d-complement10 frozen sources, four-test receipt, small/target
receipts and fields, comparisons and once-only audit. Use the existing accepted
receipt/field for this exact case; supervisor retains an existing run rather than
launching a duplicate. Next distinct tests require declared geometry and their own
immutable identity/receipts. Broader object/native qualification remains active.
