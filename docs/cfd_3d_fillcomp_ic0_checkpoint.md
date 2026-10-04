# Positive omitted-fill compensation: stable setup, rejected convergence

Six independent support controls pass, including a coupled SPD cycle with actual
omitted fill: factor-model minus rounded predictor is PSD, the inverse is bounded
relative to its physical inverse, and no late pivot shift is required. Original
FE/load/pressure/action, coarse reproduction, lifecycle and admission checks pass.

The one original4992tet full control reached3000iterations/39.347s/owned328.531MiB
(sampled327.469MiB). Full independently reconstructed original FE residual is
0.7107506381443139: momentum0.710749918876992, continuity0.001011157208224729.
The same retained blocks stall; this is a convergence failure, not a memory/time
failure. No field/force is published. Small usefulness, larger control and finer
mesh admission are rejected; do not retry unchanged or raise caps.

Factor owns23.940MiB and compensates8,935,210 omitted pairs (sum2,835,573.129;
max131.007). No late pivot shifts; minimum factor pivot.663375. Velocity coarse
skew2.28e-15/reproduction3.15e-14 pass. Setup1.057s and solve31.836s show that the
memory-saving construction is cheap but the smoother/coarse combination is too
weak. Global quadratic velocity30 reproduces its span; it does not cover the
remaining velocity modes. Pressure complement10 remains unchanged, but its prior
qualification with the complete Cholesky velocity inverse does not transfer.

Complete original mesh/matrix/RHS identities agree with the accepted small cube;
all original pressure modes/residual/force/resource gates and protected native
build hashes remain. No native/default adoption. Persistent broader goal active.

Next: retain the successful complete-Cholesky/pressure recipe. A distinct mesh
screen may reduce only remote streamwise slab count while keeping the finer cube
surface and near-body normal planes. Apply every existing geometry non-degradation
gate on both domains before one fresh factor-admission attempt. If that is rejected,
a stronger distributed velocity coarse space or bounded-fill factor is needed;
late-pivot repair and global-polynomial-only correction are insufficient.
