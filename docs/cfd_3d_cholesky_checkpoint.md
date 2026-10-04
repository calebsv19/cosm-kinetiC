# Sparse symmetric factor resumes bounded force-convergence testing

2026-10-03. Persistent Main Edit reference checkpoint. The exact previously
memory-stopped 13824-tetrahedron L4 body4 normal-refinement cube now completes
under the original numerical and resource gates. This achieves the bounded
ability to resume force-convergence testing. Cube physical qualification and
the broader object/wind-tunnel goal remain open.

The optional macOS reference factor uses the installed Accelerate sparse Cholesky
API. No dependency was installed. The adapter stores one symmetric triangle,
uses explicit nested-dissection ordering, copies directly from symmetric CSR
rows instead of materializing a full coordinate conversion or velocity-block
copy, and releases its factor after solving. The zero reconstruction cache
recomputes exact local elimination coefficients; it does not drop pressure modes
or change equations. The adopted reference defaults are Cholesky, METIS ordering,
zero reconstruction cache and 128-cell quadrature batches. Original/older runners
remain unchanged; this is not a native solver or public first-start change.

Five independent support tests cover dense inverses on anisotropic matrices,
fixed linear action, input preservation, both orderings, a velocity prefix of an
indefinite mixed operator, non-SPD/singular rejection, invalid input and 100 repeated
cleanup lifecycles. Test receipts bind the shim source, Python adapter, tests and
exact binary. The local runner freezes source, C shim, compiled library, build
command and compiler/platform/SDK identity into each immutable receipt. Failed
factors never silently fall back. Snapshot observation selects the exact result
and field from receipt arguments, avoiding ambiguity with the build JSON artifact.

The initial larger AMD control reached the unchanged memory cap and is retained
without an accepted field. Intermediate controls improved speed while using more
memory than the prior uncondensed method; they are recorded honestly. Removing
triangle conversion and reconstruction caching produces measured total savings
on the matched body4 case. Before admitting refinement, an independent tighter
uncondensed solve verifies original mesh, RHS/free-DOF identity, raw loads and full
fields. Maximum field differences are 2.12e-11 velocity and 9.44e-8 Pa pressure.
The older less-tightly-solved field differs by 2.02e-6 Pa and is not used as the
tighter pointwise comparator. The tighter control's full residual is 6.88e-12;
it does not attain its requested 1e-12 target, though all numerical gates pass.

The final adopted default invocation also verifies the original 4992-tet cube:
150 iterations, 13.47 seconds, 726.6 MiB and original full residual 1.06e-11.
Against the prior 32.02-second/832.4-MiB original control, this is 2.38 times
faster with 12.7% less observed peak memory. Its operator and raw loads match.

| L4 body4 control | Tetrahedra | Iterations | Total time | Observed peak RSS |
| --- | ---: | ---: | ---: | ---: |
| Prior uncondensed base | 10752 | 580 | 77.51 s | 1511.5 MiB |
| Fresh tighter uncondensed base | 10752 | 701 | 82.18 s | 1428.6 MiB |
| Adopted settings, matched base | 10752 | 140 | 38.34 s | 1413.2 MiB |
| Newly admitted normal refinement | 13824 | 140 | 58.28 s | 1577.4 MiB |

The matched base saves 6.5% relative to the retained default control, with a smaller
1.1% margin relative to the fresh tighter control. Do not claim a large or robust
memory improvement from these base timings alone. The concrete admission result
is that the exact formerly stopped normal case now completes, including full
residual evaluation, physical diagnostics and post-serialization resource checks.
Its original full residual is 9.17e-11; volume divergence is 5.17e-10, flux error
1.09e-10 and energy imbalance 1.15e-10. The 50000-tet, 1800-MiB, 180-s,
3000-iteration and original residual/conservation/energy gates are unchanged.

| Base to first-normal refinement measurement | Relative change |
| --- | ---: |
| Raw pressure force | 0.318% |
| Raw symmetric viscous force | 0.887% |
| Independent reaction | 0.325% |
| Inlet pressure / dissipation | 0.226% / 0.227% |
| Raw surface force versus reaction mismatch | 3.007% to 2.182% |

Individual changes in this one normal pair are below 1%, but the separate
raw/reaction criterion still fails. This is not a force-error bound, not L8 domain
qualification and not evidence for inertial wakes or arbitrary objects.

The independent observer verifies component identities to 2.12e-14 N. Volume
strong-equilibrium defect decreases 0.15767 to 0.14074 and interior stress-jump
norm 0.02167 to 0.01905. It preserves raw pressure/viscous loads and reports the
remaining stress-resolution gap; its indicator is not a certified force bound.

Run `make test-cfd-reference3d-cholesky` for optional macOS factor support and
`make audit-cfd-3d-cholesky` for immutable source/binary/field provenance,
full acceptance, matched force/stress comparisons and preserved predecessor
workers. Runtime entrypoint is `scripts/run_cfd_reference3d_cholesky.py`; result
and measurement report is `build/c3d-cholesky/checkpoint-audit.json`. Do not
substitute these reference commands for the supported public headless quickstart.

Next bounded work: repeat the matched base/normal pair on L8 to separate domain
sensitivity, then use independently measured component stress defects to select
a further body/normal/outer resolution control within the same caps. Close the
remaining raw/reaction gap and verify all separate force/scalar gates before
native corrections and authored physical stationary-object validation. Transient,
outlet, inertial wake, curved/moving/free-surface and atmosphere qualification
remain subsequent requirements. Shared FE/factor/job extraction is reuse-deferred;
no shared API/version, native source, package, install, commit or release changed.
