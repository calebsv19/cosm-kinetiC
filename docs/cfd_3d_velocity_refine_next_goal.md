# Next: pressure weak-mode coverage diagnostic on the unchanged exact cube

Cubic20 worsened target cost; a fixed velocity accuracy correction improved inner
residuals but failed whole target time. Investigate coarse pressure placement
before another solve. Sample a declared basis containing all ten existing global
pressure directions and spatially localized pressure directions. Use the original
Float velocity inverse and original Double coupling/pressure action, with pressure
mass scaling. Report projected Schur Rayleigh values, symmetry, eigenvector
localization and residual outside the sampled span; include geometry conditioning
and approximation error checks. This is approximate preconditioner diagnostics,
not a complete FE inf-sup/nullspace certificate or accepted flow field.

Keep exact33216tet geometry, physical coefficients, FE modes/quadrature, matrix/RHS
hash identities and1800MiB/180s caps. Conservative diagnostic workspace is reserved
at fresh stage admission. Immutable supervisor/inputs/results and original native
hashes stay required. One small support/control precedes one exact target diagnostic.
Use results to choose localized pressure enrichment only if observations justify it.
Broader goal remains active; force convergence still needs a useful strict target
recipe and separate original physical force/energy gates.
