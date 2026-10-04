# Exact symbolic factor cost and rejected graph controls

2026-10-03. Persistent Main Edit reference diagnostics. These runs analyze graphs
and resource requirements; they publish no numerical field, do not prove positive
definiteness/convergence and do not certify physical force accuracy.

A separate optional C shim exposes the installed explicit symbolic SparseFactor
API and its public factorSize_Double/workspaceSize_Double byte requirements. It
owns and cleans up symbolic handles, validates array structure and preserves inputs.
Seven independent tests cover scalar bijections/coefficient/action preservation on
anisotropic/refined FE controls, complete component-pair block coverage, agreement
with actual exact-factor storage, owned lifetime/100 cleanups, invalid bounds/options,
explicit absence of numeric/SPD proof and full diagnostic JSON serialization. The
initial complete diagnostic fails on a NumPy DOF count; its immutable receipt/log
are retained and reporting-only integer conversion plus a serialization guard fix
it. Frozen source/C library/build/SDK header/API evidence binds the final controls.

| Diagnostic graph | Factor storage MiB | Numeric workspace MiB | Owned analysis peak MiB |
| --- | ---: | ---: | ---: |
| L4-body6-base-component-fixed | 852.7 | 21.9 | 1010.9 |
| L4-body6-base-interleaved | 869.6 | 21.9 | 1278.9 |
| L4-body6-base-vector | 853.7 | 39.2 | 1207.3 |
| L4-body6-normal-component | 1264.1 | 25.7 | 1255.0 |
| L4-body6-normal-interleaved | 1230.5 | 25.7 | 1719.0 |
| L4-body6-normal-vector | 1274.4 | 47.5 | 1471.5 |

Factor/workspace byte requirements exclude live FE/input/symbolic/solver allocations
and are not process RSS predictions. The component-major base estimate exactly
matches the completed predecessor's 894133240-byte factor. The exact stopped normal
needs 1325505336 bytes of factor storage and only 26931968 bytes of numeric workspace.
Factor fill dominates this known requirement; reducing the small workspace alone
cannot be treated as a complete memory solution.

All six successful diagnostics retain the original accepted-base or failed-normal
mesh/free-DOF/RHS identity. Original mixed inputs, pressure blocks and couplings
remain unchanged. Scalar interleaving is a bijective component coordinate change
and preserves every stored coefficient and symmetric action. The vector graph
joins every node pair with any component coupling and honestly models dense
three-component blocks; its graph values are structural markers, not replacement
physical coefficients. No rows, pressure modes or tiny coefficients are pruned.

On the normal mesh scalar interleaving saves only 2.65% factor storage while
raising conversion/analysis owned peak to about 1719 MiB. Keeping both the original
operator and permuted factor input would add live storage; no robust completed-run
headroom is established. The complete vector graph raises factor and workspace
requirements. Reject both as implemented and do not launch numerical factors just
to repeat a resource failure. Existing accepted exact-factor runners remain intact.
No normal field or new force result exists from these symbolic controls. Physical
component/domain/raw-reaction/stress qualification and Stage 1 remain open.

Evidence is under `build/c3d-symbolic/`: seven support proofs, six successful
symbolic diagnostics, one retained serialization failure and `checkpoint-audit.json`.
Use `scripts/run_cfd_reference3d_symbolic.py` with an explicit graph mode for the
optional diagnostic. Local Make lanes are `test-cfd-reference3d-symbolic` and
`audit-cfd-3d-symbolic`. Prior numerical fields, failed trials, native workers and
all source remain preserved. No native/shared API, dependency, version, commit,
package, install, release or deployment change. Generic FE/factor/job extraction
stays deferred.

The next [bounded exact-component Cholesky/symmetric-sweep investigation](cfd_3d_component_cholesky_goal.md)
revisits the prior near-converged, time-stopped coupled component sweeps using the
faster measured exact sparse Cholesky backend. Preserve all off-component couplings,
prove fixed SPD action and original equations, measure completed original-cube
acceptance/cost, then extend only useful controls to the admitted base and stopped
normal. Numerical admission still precedes separate physical reference qualification,
then native traction, authored objects and transient/outlet/inertial-wake scope.
The full persistent object/wind-tunnel goal remains active.
