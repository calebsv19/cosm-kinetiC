# Inner CG8: pressure coarse setup rejection

Ten independent controls pass (including transformed SciPyCG8, physical FE bits,
Galerkin, Float coarse refinement, owners/admission and source transform). One
original4992tet trial, bundle1e696fd902a93727a164c86be10d9bd6ca2a09e863e83267638706c0380bf82d,
returns1 after6.717320959s/sampled363.03125MiB. Both velocity factors admitted
431302623bytes. No outer iteration, output JSON or numerical field. Nonlinear
RHS-dependent CG8 fails unchanged global ten-mode pressure Schur symmetry gate
inside pressure-complement10 setup. No gate relaxed or large/finer trial allowed.
This is a setup compatibility rejection, not a full convergence measurement.

The pressure proxy was built using the nonlinear velocity inverse, whose columns
need not form a symmetric matrix. Preserve rejection and sources/receipt. Next
separate candidate builds pressureten W with the already validated P3/local triple
fixed correction (same original physical velocity/Galerkin/factors) and applies
CG8 only through flexible outer velocity solves. No extra factor/basis or physical
pressure mode removal. Full strict/cost/pressure gates still decide acceptance.
Broad user goal active; no native/default/force certification.
