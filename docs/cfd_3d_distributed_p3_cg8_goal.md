# Cubic coarse correction with eight-step inner preconditioned CG

Predecessor40916d5cffe99d983f6e48d95c910da7a597834bc80a200757a44c6314048d8f:
P3+three-step local correction gives strict1.061e-11 in498iter but whole43.358s
fails23.502s usefulness. Preserve its strict field and physical force equivalence.
Distinct candidate changes local inverse only to eight preconditioned CG steps
on ORIGINAL Float64 velocity with the SAME controlled-fill factor. Startszero,
checks finite/positive r.G.r and p.A.p, fixed8 unless exactlyzero residual, no
inner tolerance relaxation/restarts. Nonlinear RHS-dependent inverse expressly
requires existing flexible right Arnoldi with both V/Z and true original checks;
no claim of exact linearity, bilinear symmetry or full spectral certification.
Same macroP3/essential projection/local physical Galerkin/Float coarse three
residual corrections, pressureten complement10, restart30, full1e-10/retained1e-11,
force/flux/div/energy/publication and1800MiB/180s/3000iter/50000tet. No extra fill.

Reuse unchanged P3 assembly/ABI/factor and support, freeze them as dependencies.
Work reservation8*(40*nv+24*nc)+2MiB covers nested CG vectors and coarse Float
conversion/residual scratch. Combined fresh numeric admission still includes both
factor storage, max scratch, actualRSS of ALL inputs, restartbothbases/work,
pressure reserve and32MiB before BOTH numeric factors. All completed arrays/factors
retire before independent complete original FE reconstruction and physical output.
Support tests validate inner8 against independent scipy CG on symmetrically
preconditioned dense model, homogeneity/positive curvature/failure guards,
original anisotropic FE/load/Galerkin invariance, partial/repeatedowners/budgets,
actual corrected Float coarse accuracy and source exact one-change transform.
Sampled coarseaction<=1e-10/reproduction<=1e-8 remains before full solve.

One original4992tet full first. Require strict residual, original identity/forces/
scalars within1e-7 and whole<=23.5018023327s. Only useful small permits matched
28416tet L4, whole<=106.642085s and>=300MiB complete velocityfactor/work saving
versus873198472bytes. Only useful matched permits saved qualitypassed43008tet
finer L4. No unchanged rejected reruns/cap relaxation, native/default adoption,
mesh/force certification. All measurements immutable; broad user goal active.
