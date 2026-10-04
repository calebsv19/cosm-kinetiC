# Exact component Cholesky sweep checkpoint

Six support tests prove exact principal inverses, fixed sweep symmetry, linearity,
positivity and majorization, original FE/RHS/pressure reconstruction, partial
failure cleanup and 100 repeated owned-handle lifecycles. All couplings remain.

The original 4992-tet L4 cube with four fixed sweeps passes the unchanged full FE,
continuity, divergence, flux, energy, resource and atomic publication gates:
2112 iterations, 175.83095 seconds, 336.09375 MiB owned peak. Full relative residual
is 3.44818e-10 (momentum 3.44818e-10, continuity 6.20895e-13). The requested 1e-10
full-residual target is not attained; the independent unchanged 1e-8 numerical
acceptance gate passes. Library MINRES success alone is not acceptance.

Compared with the accepted exact coupled factor, owned memory falls 53.74%, but
wall time increases 13.06 times. Exact component factors occupy 65029776 bytes.
Velocity/pressure maximum field differences are 2.19313e-12/4.15821e-9; force and
scalar differences are below 1.1e-10 relative. Raw/reaction mismatch remains
3.64358%, so physical qualification remains open. The numerical field is retained;
this path is rejected for larger runs because the original is already near 180 s.

Evidence: `build/c3d-component-cholesky/checkpoint-audit.json` and the frozen
`original-L4-four-sweeps` receipt/field. Predecessor source/evidence and protected
native workers remain unchanged. No commit, package, install or native API change.

The next declared gate is [two coupled principal blocks](cfd_3d_block_cholesky_goal.md).
The full object/wind-tunnel goal and Stage 1 are open.
