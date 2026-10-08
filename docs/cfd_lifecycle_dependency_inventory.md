# Current reference test dependencies and historical audits

Updated 2026-10-07 during lifecycle repair. Current tests construct their own
small numerical inputs. The full `test-cfd-reference-current` target declares
all 19 distinct tracked Accelerate support libraries under the selected
`BUILD_DIR/cfd-reference-support`. It requires the reference environment plus
explicit AMG setup (`make cfd-reference-amg-env`). No saved campaign selection
is performed by the current group.

## Tracked support identities

| Former support path | Tracked source stem |
| --- | --- |
| `c3d-block-ic0/support/factor.dylib` | `scripts/cfd_reference3d_block_ic0.c` |
| `c3d-bounded-fill1/support/factor.dylib` | `scripts/cfd_reference3d_bounded_fill1.c` |
| `c3d-cholesky/support/factor.dylib` | `scripts/cfd_reference3d_accelerate.c` |
| `c3d-distributed-p2/support/coarse.dylib` | `scripts/cfd_reference3d_distributed_p2_coarse.c` |
| `c3d-distributed-p3/support/coarse.dylib` | `scripts/cfd_reference3d_distributed_p3_coarse.c` |
| `c3d-encoded-operator/support/factor.dylib` | `scripts/cfd_reference3d_encoded_storage.c` |
| `c3d-fillcomp-ic0/support/factor.dylib` | `scripts/cfd_reference3d_fillcomp_ic0.c` |
| `c3d-local-cost-split/support/factor.dylib` | `scripts/cfd_reference3d_local_cost_split.c` |
| `c3d-mixed-precision/support/factor.dylib` | `scripts/cfd_reference3d_mixed_storage.c` |
| `c3d-native-inner8/support/factor.dylib` | `scripts/cfd_reference3d_native_inner8.c` |
| `c3d-p3-cg8-scalar/support/coarse.dylib` | `scripts/cfd_reference3d_p3_coarse_scalar.c` |
| `c3d-packed-inner8/support/factor.dylib` | `scripts/cfd_reference3d_packed_inner8.c` |
| `c3d-packed-physical/support/factor.dylib` | `scripts/cfd_reference3d_packed_physical.c` |
| `c3d-priority-fill1/support/factor.dylib` | `scripts/cfd_reference3d_priority_fill1.c` |
| `c3d-pruned-graph/support-factor.dylib` | `scripts/cfd_reference3d_mixed_storage.c` |
| `c3d-second-normal/scalar-support-factor.dylib` | `scripts/cfd_reference3d_mixed_storage.c` |
| `c3d-seed-fill1/support/factor.dylib` | `scripts/cfd_reference3d_seed_fill1.c` |
| `c3d-symbolic/support/symbolic.dylib` | `scripts/cfd_reference3d_symbolic.c` |
| `c3d-tight-fill-bound/support/factor.dylib` | `scripts/cfd_reference3d_tight_fill_bound.c` |
| `c3d-vector-storage/support/factor.dylib` | `scripts/cfd_reference3d_vector_storage.c` |
| `c3d-workspace-cholesky/support/factor.dylib` | `scripts/cfd_reference3d_workspace_cholesky.c` |

## Saved experiment assertions

These checks remain under `tests/archive_cfd_reference3d_*.py` and are excluded
from current discovery. Ordinary saved-tree audits require an explicit
`PHYSICS_SIM_CFD_ARCHIVE_ROOT` whose contents correspond to the former build
root. The force/transition/graded/selective diagnostic families use the exact
input variables declared in their modules. Missing inputs fail explicitly.
Historical source-transform checks can also fail on current source drift;
passing current algebra does not make an old checkpoint audit pass.

- `tests/archive_cfd_reference3d_accuracy_admission_guard.py`
- `tests/archive_cfd_reference3d_accuracy_first.py`
- `tests/archive_cfd_reference3d_accuracy_graded.py`
- `tests/archive_cfd_reference3d_accuracy_physics.py`
- `tests/archive_cfd_reference3d_adaptive_cg8.py`
- `tests/archive_cfd_reference3d_distributed_p2.py`
- `tests/archive_cfd_reference3d_distributed_p3.py`
- `tests/archive_cfd_reference3d_distributed_p3_cg8.py`
- `tests/archive_cfd_reference3d_distributed_p3_cg8_pressure.py`
- `tests/archive_cfd_reference3d_force_local.py`
- `tests/archive_cfd_reference3d_force_transition.py`
- `tests/archive_cfd_reference3d_four_interval.py`
- `tests/archive_cfd_reference3d_graded_edge.py`
- `tests/archive_cfd_reference3d_local_cost_split.py`
- `tests/archive_cfd_reference3d_native_inner8.py`
- `tests/archive_cfd_reference3d_p3_cg16_scalar.py`
- `tests/archive_cfd_reference3d_p3_cg8_scalar.py`
- `tests/archive_cfd_reference3d_packed_inner8.py`
- `tests/archive_cfd_reference3d_packed_matched.py`
- `tests/archive_cfd_reference3d_packed_matched_profile.py`
- `tests/archive_cfd_reference3d_packed_physical.py`
- `tests/archive_cfd_reference3d_packed_physical_matched.py`
- `tests/archive_cfd_reference3d_pressure_coverage16.py`
- `tests/archive_cfd_reference3d_pressure_proxy_refine3.py`
- `tests/archive_cfd_reference3d_priority_fill1.py`
- `tests/archive_cfd_reference3d_seed_fill1.py`
- `tests/archive_cfd_reference3d_selective_mesh.py`
- `tests/archive_cfd_reference3d_tight_fill_bound.py`
- `tests/archive_cfd_reference3d_translated_cells.py`

The batch-two assertion inventory records each previous, current and archived
assertion count against HEAD; there are no lost assertion counts. This is a
migration audit, not a substitute for running the numerical tests or the
historical campaigns. The analytic traction receipt now writes to a unique
TemporaryDirectory. The method-receipt tests deliberately create an isolated
fake build/venv tree as their fixture; that is not a historical dependency.

## Remaining runner migration

Many specialized campaign runners and Make recipes still embed build paths.
Only the migrated active box/cube/manufactured accuracy workflows and current
reference test group have demonstrated isolation. Saved qualification cannot
be recreated by changing a path or relaxing a digest. Independent cube-reference
qualification and broader campaign output migration remain separately open.
