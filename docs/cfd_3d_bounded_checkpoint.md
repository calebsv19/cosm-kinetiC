# Bounded exact assembly admits the finer-body force control

2026-10-03. Persistent Main Edit, optional local reference checkpoint. Direct
assembly on free retained variables lowers the matched normal cube's owned peak
memory by 18.7% with essentially unchanged total time. The exact 18816-tet body6
base that previously stopped during factor setup now completes under all original
numerical and resource gates. Its force/stress measurements remain physically
unqualified; Stage 1 and the full object/wind-tunnel goal remain open.

The new app-owned reference class inherits original local mixed elimination,
RHS reduction, pressure/velocity reconstruction and independent original FE
residual authority. It changes global sparse construction: bounded COO batches
feed hierarchical CSR sums directly on free retained variables, avoiding full
retained matrix assembly followed by row and column slicing. Default batches
contain 64 macros and allocate 10863616 bytes of COO arrays; the old body6 base
allocated 798475776 bytes. Live CSR merges and sparse-library workspaces remain
additional costs. The tracked CSR estimate is not total memory or owned RSS.
No pressure penalty, mode truncation, force replacement or threshold dropping
is introduced. Floating accumulation order changes the matrix checksum and can
change tiny cancellations/sparsity/factor ordering; physical equivalence is
verified rather than asserting bitwise identity of the assembled matrix.

Five independent support tests pass. They compare full/reduced sparse action
across multiple batch sizes on anisotropic and conforming refined controls,
fixed-boundary projection/symmetry, nonzero eliminated RHS and reconstructed
fields, explicit original quartic quadrature, invalid-batch rejection and exact
preservation of tiny macro pressure-diagonal terms. Support receipts bind source,
test and log hashes. Frozen runtime receipts bind source, the existing C factor
shim, compiled library, compiler/platform/SDK/build identity and all artifacts.
The installed factor library and original FE/reconstruction sources are unchanged.

| Matched L4 count4 normal control | Fresh legacy | Bounded assembly |
| --- | ---: | ---: |
| Tetrahedra | 13824 | 13824 |
| Total job time | 74.65 s | 75.06 s |
| Owned peak RSS | 1580.7 MiB | 1285.7 MiB |
| Periodically observed peak RSS | 1569.2 MiB | 1285.7 MiB |
| Iterations | 140 | 150 |

The 18.66% owned-memory reduction is measured against a fresh control; total time
increases 0.55%. Against the older historical control, both runs were slower, so
no speedup is claimed. Original mesh/free-DOF/RHS hashes match. Maximum new-versus-
fresh coefficient differences are 2.09e-11 velocity and 5.27e-8 Pa pressure; all
force/scalar differences are below 4.7e-10 relative. The new original full residual
is 7.58e-12 versus 7.83e-11 on the fresh legacy field. Both satisfy the same target
and gates; the different residual/iteration endpoints reflect floating accumulation
and fixed callback sampling, not a tolerance change.

A same-backend count4 base also reproduces the older accepted base forces/scalars
to 6e-13 relative. Original-cube default invocation passes in 13.97 s / 792.1 MiB,
with matched fields/forces. Its historical Cholesky control was 13.47 s / 726.6 MiB.
This small-case memory increase is retained as a tradeoff. Bounded assembly is
adopted only in the new optional large-mesh runner; earlier reference defaults
and the supported public headless quickstart remain unchanged.

The exact formerly stopped count6 base preserves its original mesh/free-DOF/RHS
identity and now completes in 124.39 s, 160 iterations and 1745.8 MiB owned peak
(1743.6 MiB periodically sampled). The original 50000-tet, 1800-MiB, 180-s and
3000-iteration caps and all numerical/physical thresholds are unchanged. Full
residual is 3.15e-11; physical maximum divergence is 5.78e-10, flux error 1.42e-10
and energy imbalance 1.25e-10. Factor storage is 894133240 bytes. All diagnostics
and post-serialization resource checks pass, and its numerically accepted field
is atomically published. The prior 11.64-s/2001.9-MiB resource failure remains
immutable and has no field. Successful assembly's peak need not decrease on every
mesh: CSR merge workspaces and factor ordering affect total cost. Admission is
established by the completed full run rather than the COO-size reduction alone.

| Same bounded backend: count4 base to count6 base | Relative change |
| --- | ---: |
| Raw pressure force | 1.056% |
| Raw symmetric viscous force | 0.542% |
| Independent reaction | 0.0091% |
| Inlet pressure / dissipation | 0.0060% / 0.0114% |
| Raw surface versus reaction mismatch | 3.007% to 2.171% |

Pressure sensitivity and the separate raw/reaction criterion remain above 1%.
Independent observers reproduce component stress identities to 2.46e-14 N.
Global strong volume defect increases 0.15767 to 0.19848 and interior jump norm
0.02167 to 0.02545. The nearer reaction agreement alone does not establish improved
overall stress accuracy. Count6 stays an experimental physical control; no mesh
is promoted as qualified. These are fixed low-Reynolds-number cube references,
not arbitrary-object, transient or inertial-wake qualification.

Runtime entrypoint is `scripts/run_cfd_reference3d_bounded.py`; e.g. supply
`--name <case> -- --length 4 --count 6` for the finer base. The observer runner is
`scripts/run_cfd_reference3d_bounded_equilibrium.py`. Use
`make test-cfd-reference3d-bounded` and `make audit-cfd-3d-bounded` for the support
and immutable evidence lanes. The report is `build/c3d-bounded/checkpoint-audit.json`.
Five solver receipts, two same-backend stress observations, independent field
comparators and all preserved predecessor/native-worker checks pass. No shared/
native API, dependency, version, commit, package, install, release or deployment
changed. Generic FE/factor/job extraction remains reuse-deferred.

The finer base has only about 54 MiB of memory headroom. The next declared slice
is [exact symmetric triangle storage](cfd_3d_triangle_goal.md): prove symmetric
operator/factor action and pressure-diagonal retention, measure matched body4/body6
fields and total memory, then attempt count6 normal refinement if headroom is
actually demonstrated. Extend the same near-body mesh to L8 afterward. All
separate force/scalar/domain/raw-reaction requirements must pass before native
traction correction and authored stationary-object qualification. Transient,
outlet, inertial wake and further geometry/material lanes remain subsequent.
