# Next: fixed physical-Double correction inside Float velocity preconditioner

Cubic20 pressure remains rejected: exact cube time180.035s and no field; last logged
500iter constant continuity4.51e-11 vs momentum3.03e-12. Preserve all original ten/
twenty/general performance and raw physical failures. Inspection finds no prior
fixed inverse refinement source in this lane; previous Float controls demonstrate
larger-than-assumed anisotropic one-step error, corrected by full flexible outer.

New preconditioner-only hypothesis: for physical velocity A and Float solve G,
y=Gx, r=x-Ay in original Double action, return y+G r. One fixed correction only,
no inner tolerance/stopping adaptation/equation/physical precision change. Float
calls remain approximate/nonlinear; flexible right outer/full reconstructed FE
residual remains authority. Use the sealed ten-column pressure basis; build its
Schur columns with the same corrected velocity action used in outer PC. Measure
true physical velocity residual before/after on first ten coarse calls only,
without extra full matrices/decoded coefficient buffers. Add conservative64*nv
bytes reservation for extra correction/action/solve vectors to fresh factor-stage
guard, beyond both Arnoldi bases/work, coarse/factor/scratch/32reserve. Include all
setup/cost/owned/sample peaks and cleanup of the new inverse owner before FE restore.

Meaningful tests: actual Float factor residual/error reduction, ideal linear-SPD
polynomial2G-GAG, anisotropic full FE/load/pressure/reconstruction/ownership and
strict corrected velocity residual, invalid/nonfinite/closed/lifecycle/reservations.
One small full numerical control before one new exact cube. Original1e-10 full/
1e-11 retained, flux/div1e-8/energy.03,1800MiB/180s/3000iter/50000tet and all mode/
bit/publication checks unchanged. Compare exact target accepted mass and ten-column
physical identities/output<1e-7; require>=10percent useful iterations OR setup+solve
benefit vs mass and whole/owned<=1.1 both, with no>10percent setup+solve regression
vs ten-column. No retries. Only full useful target proof qualifies exact reference
recipe; general/native/default remain separate. Broader object/force goal active.
