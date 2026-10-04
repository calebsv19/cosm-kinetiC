# Complete mixed storage with shared exact-factor input

2026-10-03. Persistent Main Edit reference checkpoint. Force-convergence testing
is operational on admitted L4/L8 finer-body cubes; physical qualification and the
full object/wind-tunnel goal remain open.

The optional reference operator separates the upper mixed storage into its full
velocity triangle, velocity/pressure rectangle and pressure triangle, and applies
both coupling directions with exact symmetric block action. The factor shares
the existing contiguous velocity row/value arrays instead of duplicating them.
Only int64 column starts are copied. Strong input owners remain alive through
cleanup; streaming hashes verify every factor input is unchanged after creation
and solve. The existing C factor, original local elimination/reconstruction,
pressure mass, full FE quadrature/residual, raw surface loads, reaction, energy,
conservation and atomic-publication authority remain unchanged.

Eight independent proofs cover anisotropic/refined full operator action and
symmetry, general pressure off-diagonals/couplings, bitwise tiny pressure diagonals,
nonzero eliminated RHS and original full FE equations, dense and FE exact inverse,
both orderings, actual shared array identity, input preservation, caller-owner
lifetime, malformed C-input bounds/types/pointers, invalid/non-SPD/singular rejection
and 100 owned cleanup lifecycles. Frozen receipts bind source, test log and exact
existing test binary. Each numerical run separately freezes source, C shim,
compiled binary, compiler/SDK/platform/build command and resource supervision.

| Matched case | Prior triangle owned peak | Shared-input owned peak | Prior / new total time |
| --- | ---: | ---: | ---: |
| L4 count6 base, 18816 tets | 1605.4 MiB | 1482.4 MiB | 131.68 / 112.70 s |
| Matched L8 count6 base, 18816 tets | 1784.3 MiB | 1685.9 MiB | 144.18 / 130.52 s |

The L4 matched control saves 7.66% owned peak memory and 14.42% measured total time;
L8 saves 5.51% memory and 9.47% total time. These are completed source-reference
measurements, not universal/native performance claims. The L4 factor shares
140724096 bytes of row/value arrays and allocates 1008776 bytes for column starts;
its factor storage remains 894133240 bytes. All mixed entries remain stored.
Earlier optional runners remain available and their defaults are unchanged.

The original mesh/free-DOF/RHS identity matches each triangle predecessor. L4
maximum field changes are 7.89e-15 velocity and 9.75e-13 Pa pressure, with force/
scalar changes below 1.58e-14 relative. L4 finishes at 160 iterations with full
original FE residual 3.15e-11, maximum physical divergence 5.78e-10, flux error
1.42e-10 and energy imbalance 1.25e-10. L8 finishes at 380 iterations with full
residual 1.05e-10, divergence 2.64e-11, flux 8.21e-12 and energy imbalance 8.36e-12.
The L8 result does not quite attain its requested 1e-10 stopping target but passes
the unchanged independent 1e-8 numerical acceptance limit and every other gate.

The measured L4 headroom authorizes the exact stopped 23616-tet normal trial.
Its original mesh/free-DOF/RHS hashes match the prior triangle failure. Assembly
completes at 1178.4 MiB owned high-water; the supervisor stops factor setup after
15.11 s at 1827.8 MiB observed RSS. There is no factor-ready record, completed
solve, accepted field or force result. All original 50000-tet/1800-MiB/180-s/
3000-iteration caps and physical/numerical thresholds stay unchanged. The retained
failure is not retried as a sampling/allocator fluctuation workaround, and larger
L8 normal is not run while this exact L4 normal remains inadmissible.

Matched L4-to-L8 pressure/viscous/reaction force changes remain approximately
2.115%/0.549%/1.440%; raw surface/reaction gaps remain 2.171%/2.063%. Pressure,
reaction/domain and separate raw/reaction criteria exceed 1%; no mesh is promoted
as physically qualified. Total inlet pressure/dissipation vary with tunnel length
and are reported as domain responses rather than flat-pressure accuracy gates.

Independent immutable-field stress observers close pressure/viscous component identities to 2.51e-14 N. L4 volume/jump defects are 0.198478/0.025447; L8 values are 0.425325/0.045707, matching their triangle predecessors. These diagnostics preserve the unresolved stress gap and are not certified force-error bounds. L8 field changes are 5.16e-14 velocity and 2.59e-10 Pa pressure; all matched force/scalar changes are below 6.40e-13 relative.

Evidence is under `build/c3d-shared-factor/`: eight support proofs, immutable
matched readiness, two accepted solver receipts and the exact finer-normal
resource failure. The runtime runner is `scripts/run_cfd_reference3d_shared_factor.py`
and independent observation uses `scripts/run_cfd_reference3d_shared_factor_equilibrium.py`.
Make support/audit lanes are `test-cfd-reference3d-shared-factor` and
`audit-cfd-3d-shared-factor`. Prior receipts, fields, workers and source are
preserved. No native/shared API, dependency, version, commit, package, install,
release or deployment changes occur. Generic FE/factor/local-job extraction stays
reuse-deferred; these remain macOS low-Reynolds-number reference controls.

The next predeclared slice is [factor peak and explicit allocator pressure control](cfd_3d_factor_peak_goal.md).
Measure whether freed assembly scratch contributes to setup RSS, prove live inputs
and equations survive any pressure call, compare completed matched total cost,
then retry the same normal mesh only with demonstrated headroom. If ineffective,
reject the control and investigate actual symbolic/factor workspace cost. Full
pressure/viscous/reaction/scalar/refinement/domain/raw-reaction criteria and stress
diagnostics must still qualify the reference before native traction corrections,
authored stationary objects, transient/outlet/inertial wake and broader geometry.
The full persistent goal remains active.
