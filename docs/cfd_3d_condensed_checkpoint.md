# Exact condensation and coupled preconditioner investigation

2026-10-03. Persistent Main Edit reference work. The original filled L4 cube is
solved faster with exact macro condensation and a coupled velocity factor, but
this approach has not demonstrated total memory savings. This checkpoint does
not complete cube force qualification or the broader object/wind-tunnel goal.

The P4 velocity/DG-P3 pressure equations, natural boundary terms, raw pressure
and viscous traction, weak reaction, full residual and resource gates remain
unchanged. Local elimination removes 105 bubble velocity and 79 mean-zero
pressure coefficients per macro; one constant pressure and 102 trace velocity
coefficients remain. All eliminated coefficients are reconstructed. Original
mesh, prescribed/free-DOF and RHS hashes match. The condensed matrix has different
coordinates: no assertion that its hash equals the uncondensed matrix hash is made.

Nine focused tests include 24 manufactured solved controls across three axes,
two resolutions, pressure offsets and prescribed/natural boundaries. Maximum
velocity error is 1.04e-13, pressure error 1.20e-12, pressure traction error
5.39e-14 N and original full residual 9.43e-12. Tests also cover nonzero eliminated
loads, pressure gauge, zero reconstruction cache, incomplete factor permutation,
fixed linear action, SPD sweeps and repeated sweeps. Local elimination residual
on the physical cube is at most 6.78e-13; local Schur asymmetry is 1.11e-14.
The bounded reconstruction cache is 64 MiB. Retained unknowns reduce from
272982 to 43350; this count alone does not prove resource savings.

Successful original cube controls reproduce raw pressure/viscous/reaction loads
within 1e-7 relative of the prior uncondensed solution. Full field comparison uses
the prior tighter solution, with velocity <1e-8 and pressure <1e-6 Pa maximum
coefficient differences. Full reconstructed finite-element quadrature, including
all original pressure coefficients, verifies residuals independently of the
condensed solve's progress metric. All successful controls pass the unchanged
flux, divergence, energy and post-serialization resource gates. Raw/reaction
mismatch remains 3.6436%, failing the separate 1% physical force criterion.

| Original cube solver | Iterations | Total time | Observed peak RSS | Full residual |
| --- | ---: | ---: | ---: | ---: |
| Prior uncondensed P4 control | 610 | 32.02 s | 832.4 MiB | 8.73e-11 |
| Condensed coupled factor, storage readback | 167 | 12.74 s | 1308.4 MiB | 6.49e-12 |
| Condensed positive incomplete symmetric factor, fill 8 | 246 | 30.43 s | 1024.9 MiB | 1.08e-11 |

The coupled factor is 2.51 times faster but uses 57.2% more peak memory than the
prior uncondensed control. The incomplete factor saves memory relative to the
coupled factor while still using more than the prior uncondensed method. No finer
force case is admitted on the basis of these measurements. The original 50000-tet,
1800-MiB, 180-s, 3000-iteration and full residual <1e-8 caps are preserved.

Retained failures are part of the evidence. Component-only factors exhaust 3000
iterations with full residual 1.30e-4, dominated by momentum rather than continuity.
Lower symmetric incomplete fill factors 3 and 5 have nonpositive factor diagonals
and are rejected without flooring. A pivoted nonsymmetric incomplete factor breaks
down in GMRES; a diagonally equilibrated version also fails. A fixed coupled
vector-block multigrid cycle with translation candidates fails at the iteration
cap. A block symmetric sweep substantially improves momentum convergence but its
single-sweep full residual 7.29e-7 still fails. Four fixed sweeps approach the residual gate but reach the 180-second time cap
before completion; that resource failure is retained with no accepted field. No rejected control publishes an accepted field.
One early JSON metadata error is retained, corrected, and covered by an algebra
serialization test.

Reference support lives in `scripts/cfd_reference3d_condensed.py`, the probe,
preconditioner module and immutable bounded runner. These are reference
investigation tools, not a change to the native solver or a new public first-start
workflow. `make test-cfd-reference3d-condensed` runs the focused proofs;
`make audit-cfd-3d-condensed` validates the frozen source bundles, receipts,
accepted/rejected field publication, predecessor evidence and unchanged workers.
The durable report is `build/c3d-condensed/checkpoint-audit.json`.

The next bounded gate is a lower-memory coupled inverse with measured original
cube equivalence and full numerical acceptance. Investigate the installed macOS SDK sparse symmetric-factor API (then a
portable counterpart), or a constrained auxiliary-space method rather than
raising caps or smoothing forces. Only measured resource improvement should
admit the previously stopped finer normal pair. Then test pressure and viscous
loads separately, raw/reaction agreement, first-normal and body/outer refinements,
and L4/L8 domain sensitivity. Native pressure/traction corrections, authored
stationary objects and physical transient/outlet/inertial wake validation follow
reference qualification. Generic shared FE/job extraction remains deferred.
Native sources, older reference fields, snapshots and workers are preserved;
no commit, package, installation or release is implied.
