# Exact cube sampled pressure coverage and conditioning diagnostic

Distinct diagnostic after velocity-refinement audit249aa382b12d9f1e3ab8b2dd5b732937ffbf89db6bc8bd827d4ec50cb6e42357.
Keep all earlier cost/force/time failures. No full flow solve or field is claimed.
Use unchanged exact33216tet L8 cube, original FE/physical Float64 action and same
Float velocity factor. Sample mass-normalized approximate Schur B^T G B-D in a
fixed space: ten existing polynomials, axial cosines3..8, twenty-four axial/
transverse cosine products, eighteen Gaussian body-face/edge directions. Later
linearly dependent columns are explicitly reported/rejected; first ten never drop.
Maximum58 sampled columns. Report symmetric projected Ritz values, actual outside
span residual, ten-direction coverage, approximate coarse-preconditioned sampled
spectrum, mesh Jacobian conditions and region-local pressure energy. No sampled
Ritz vector is certified as a full Schur eigenvector or physical null/inf-sup mode.
No pressure directions/equations are removed from physical operator.

Reserve8*(8*np*64+12*nv+8*64*64) bytes beyond unchanged bothbases/work, ten coarse,
factor/scratch/currentRSS/32MiB at fresh numeric admission, even without Arnoldi.
1800MiB/180s/50000tet caps include all diagnostic actions/serialization. Original
mesh/matrix/RHS/Float identities must match sealed ten field. Validate analytic
Schur sign, scaling, Ritz outside residual, localization, rank/refusal and budget
with small independent tests. One small original FE diagnostic before one exact
cube. Preserve all inputs/frozen sources/cap receipts, close factor, verify input
hashes before/after diagnostics. Results guide the next PC; they do not authorize
adoption, pressure-error/physical certification, or bypass force convergence gates.
