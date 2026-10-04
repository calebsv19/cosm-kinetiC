# Pressure coverage and conditioning measured on exact cube

Four independent analytic tests and the small real FE diagnostic pass. The exact
33216tet L8 diagnostic completes33.403s, owned/sample1481.547MiB; fresh admission
1766.398MiB includes58-column diagnostic workspace, original bothbases/work,
factor/scratch/coarse/currentRSS/32reserve. Mesh/matrix/RHS/Float identities match
the sealed ten-pressure field. All58 prescribed sampled columns retain rank;
physical input hashes remain unchanged and factor closes. No flow field is created.

Mass-normalized approximate Schur projected values span.0152713–.0951888, sampled
condition6.233. Existing balanced-ten sampled values span.0246693–1.0088380,
sampled condition40.894. Weakest sampled vector has99.31percent energy in ten
polynomials; other weak vectors include end/wall content not covered by those
polynomials. Near-body mass energy is generally small. Outside-span relative
residuals.415–.571 are substantial: these are projected samples, NOT certified full
Schur eigenvalues, nullspace/inf-sup evidence or a diagnosis of every stalled mode.
Do not equate sampled condition with flexible-GMRES iteration count.

Jacobian condition median22.042/max271.723; within.1m body band median15.596/max48.994.
The largest geometric condition lies outside that band. This does not prove the
mesh causes a pressure mode, nor justify changing physical geometry yet.

Next distinct hypothesis: preserve the exact same ten coarse directions and their
Schur correction, multiply only the balanced complementary mass inverse by10.
Sampled pressure-vs-velocity scale disparity motivates this cheap PC-only test.
Physical Float64 action/full FE/targets/resources/force gates stay unchanged; all
cubic20/fixed-velocity/general cost failures are retained. Goal remains active.
