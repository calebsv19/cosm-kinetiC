# Explicit symmetric storage and finer reference force controls

2026-10-03. Persistent Main Edit reference checkpoint. Full force testing remains
operational on admitted cubes, but physical qualification and the broader
object/wind-tunnel goal remain open.

The new optional reference runner assembles only the upper triangle of the exact
free condensed mixed operator. Its explicit LinearOperator applies
`T*x + T.T*x - diag(T)*x`. The separate factor adapter accepts an explicitly
validated upper CSR representation and factors only its positive velocity prefix.
The original C factor library, local mixed elimination, full-field reconstruction,
FE quadrature, pressure mass, raw traction, reaction and energy authority remain
unchanged. All pressure/coupling entries are retained, including tiny pressure
diagonals. This is not a change to the native solver or supported public quickstart.

Seven independent tests cover anisotropic/refined operator action across assembly
batches, vector/column/block action and symmetry, bitwise pressure diagonals,
nonzero eliminated RHS and original full FE equations, dense and FE exact velocity
inverses, both orderings, input preservation, invalid/non-SPD/singular rejection
and 100 owned cleanup lifecycles. An initial entry-count assertion failed because
floating summation order cancels some roundoff entries to exact zero. The retained
failed log and corrected coefficient/action proof make this limitation explicit;
no threshold pruning was added. General sparse coefficients are numerically, not
bitwise, equivalent.

| Matched reference case | Prior bounded owned peak | Triangle owned peak | Prior / new total time |
| --- | ---: | ---: | ---: |
| L4 count4 normal, 13824 tets | 1285.7 MiB | 1318.9 MiB | 75.06 / 67.02 s |
| L4 count6 base, 18816 tets | 1745.8 MiB | 1605.4 MiB | 124.39 / 131.68 s |

The larger control saves 8.04% owned peak RSS while taking 5.86% longer. The smaller
control uses 2.58% more owned memory despite smaller assembly storage and lower
measured time. Factor fill and allocation lifetimes dominate total peak; there is
no universal memory or speed improvement. Adopt only the new optional path for
memory-constrained larger reference controls. Earlier runners remain available.
The bounded COO batch drops 10863616 to 5484544 bytes; CSR merge-array estimates
exclude library workspaces and are not process RSS claims.

Both original mesh/free-DOF/RHS identities match. Count4 field differences are
2.08e-11 velocity and 5.25e-8 Pa pressure; force/scalar changes stay below 4.68e-10
relative. Count6 field differences are 5.17e-15 velocity and 1.05e-12 Pa pressure;
force/scalar changes stay below 8e-15 relative. Full original FE residuals are
6.81e-11 and 3.15e-11; all numerical/resource/atomic-publication gates pass.
The count6 raw surface/reaction gap remains 2.171%, above its separate 1% gate.

Measured larger-case headroom authorizes the exact 23616-tet count6 normal trial.
Assembly completes at 1197.6 MiB owned RSS. The supervisor stops factor setup at
1889.8 MiB observed RSS after 18.03 s. There is no factor-ready record, solve,
accepted field or force result. The original 50000-tet/1800-MiB/180-s/3000-iteration
caps remain unchanged; no physical result is inferred from successful assembly.

The matched L8 count6 base holds all L4 internal X planes translated by 2 m, keeps
Y/Z nodes and connectivity, and changes only distant endpoints. It completes
at 1784.3 MiB owned peak RSS (1725.2 sampled), 144.18 s, with full residual
1.018e-10. This narrowly exceeds the requested 1e-10 stopping target, but passes
the unchanged independent full numerical acceptance limit and every other gate.
Matched L4-to-L8 pressure/viscous/reaction force changes are 2.115%/0.549%/1.440%;
raw/reaction gap improves 2.171% to 2.063% but still fails. Total inlet pressure
and dissipation rise 26.407%/26.203% with tunnel length; those are length-dependent
responses, not a flat-pressure domain-qualification criterion. Physical force
domain sensitivity and raw/reaction criteria remain unpassed. Larger L8 normal
is not attempted while the same-sized L4 normal already fails the memory gate.

Independent original stress observers verify component identities to 2.45e-14 N. L4 volume/jump defects are 0.198478/0.025447; L8 values are 0.425325/0.045707. These are unresolved stress diagnostics, not certified force-error bounds. The L4 values reproduce the bounded predecessor within numerical roundoff. Observer inputs and complete numerical gates are independently bound to the exact immutable fields.

Evidence is frozen under `build/c3d-triangle/`: support receipts, matched readiness,
three successful solver receipts and the exact normal resource failure. Runtime
entrypoint is `scripts/run_cfd_reference3d_triangle.py`; independent observation
uses `scripts/run_cfd_reference3d_triangle_equilibrium.py`. Support/audit Make lanes
are `test-cfd-reference3d-triangle` and `audit-cfd-3d-triangle`. Prior fields,
receipts, native workers and source are preserved. No shared/native API, version,
dependency, commit, package, install, release or deployment changes are made.
Generic FE/factor/local-job extraction remains reuse-deferred.

The next predeclared slice is [shared exact factor input](cfd_3d_shared_factor_goal.md):
keep complete velocity/coupling/pressure blocks, let the unchanged factor share
velocity-triangle row/value arrays with an owned lifetime, prove original action
and input preservation, then measure matched memory before retrying the stopped
normal control. Reference physical qualification still precedes native traction
correction, authored stationary objects, transient/outlet/inertial wake and more
geometry/material scope. The full persistent goal remains active.
