# Offline surface-source admission and allocation

This is the first GrowthSim/PhysicsSim communication slice for general fluid and
atmosphere behavior. It does not require a wind-tunnel scene. It admits copied
GrowthSim Fire source frames/bundles and emits conservative allocation plans.
No fluid solver is executed by the adapter, and no source is physically applied.
Thermal temperature/enthalpy, buoyancy and coupled fluid restart remain future work.

## Working commands

From PhysicsSim source, choose a fresh, explicitly owned journal directory:

```sh
python3 -B scripts/physics_sim_surface_source.py \
  --policy tests/fixtures/surface_sources/prescribed-policy.json \
  --store build/surface-source-example admit tests/fixtures/surface_sources/prescribed-patch.json
python3 -B scripts/physics_sim_surface_source.py \
  --policy tests/fixtures/surface_sources/prescribed-policy.json \
  --store build/surface-source-example allocate --sequence 0 --plan-id prescribed \
  --boundaries tests/fixtures/surface_sources/substeps.json
python3 -B scripts/physics_sim_surface_source.py \
  --policy tests/fixtures/surface_sources/prescribed-policy.json \
  --store build/surface-source-example inspect
```

The prescribed fixture uses zero producer hashes deliberately and is accepted
only by a policy explicitly pinning those values. It is an allocation known answer,
not evidence of a native combustion solve or an authenticated external producer.
A real native frame requires its own actual worker/adapter hashes and authored
calibration/configuration, all explicitly pinned by the receiver policy.

GrowthSim's `scripts/run_physics_surface_contract.py` runs its actual producer CLI
and this CLI together, admitting two native successive bundles and replaying their
exported frames. It emits source frames, admission/allocation receipts and a final
`acceptance.json` without requiring Python imports from the other live checkout.
Only the test driver selects and executes a GrowthSim worker; admission never does.

## Policy and semantics

`physics_sim_surface_source_policy/v1` binds one consumer/model ID, exact source
configuration/calibration/provenance, first native tick and receiver layer. The
receiver declares a right-handed Z-up grid with origin, positive XYZ spacings,
dimensions, layer index and explicit XY fluid mask (1 fluid, 0 blocked). It does
not infer any actual runtime geometry. The source lies on the lower XY face of
that layer, faces +Z and must be completely covered by admitted fluid cells.
Grid size is at most 262144 cells, each axis at most 256. Rotated/moving patches,
negative sinks, missing masks, partial coverage and solid intersections reject.

The clipped nodal dual rectangles are intersected conservatively with receiving
XY cells. Different XY spacings are supported through explicit area intersections;
no interpolation onto arbitrary objects is implied. Flat cell index is X-fast:
`(layer*Ny+y)*Nx+x`. Each transferred J/kg quantity is partitioned using positive
area fractions; the last contribution retains the floating-point remainder.
Geometry whose coverage cannot be established at the declared precision rejects.

Plans use increasing substep boundaries inside the source's half-open interval,
with at most 256 substeps. Cumulative quantity differences preserve uniform-rate
coverage. Each plan reports retained-before, planned and remaining-after amounts.
Only transferred energy/smoke are allocated; source generation/untransferred totals
remain in the admission receipt. The prescribed 6000 J / 0.0008 kg yields
600/1800/3600 J and 0.00008/0.00024/0.00048 kg over [0,.005), [.005,.02), [.02,.05).
The interior-node fixture maps to four equal receiver cells (1500 J each, within
binary64 area-rounding precision). Independent unequal-cell controls also pass.

An allocation plan is not a consumption record. Alternative named plans may
cover overlapping time ranges; they are never summed as applied sources. There
is no fluid applied cursor or physical checkpoint in this slice. Future application
must introduce transactional fluid-state/history/cursor/remainder publication.

## Journaling, rejection and bounds

One SQLite journal binds an immutable policy digest and a contiguous source branch.
Equal event/digest replay returns the original receipt. Changed content, sequence
gaps, time gaps and policy/provenance/calibration changes reject. A new branch or
policy needs a different journal; neither forks nor numeric equivalence migrate
an existing receiving history. Source bytes are validated before geometry admission.
The versioned numeric digest tags binary64 bits and preserves signed zero.

Admissions and plans use independent SQLite transactions with FULL synchronous
commits. Concurrent equal admissions serialize to one accepted row and equal
replays. Plan IDs similarly replay or conflict. Reopen preserves the journal/cursor;
this is adapter metadata persistence, not PhysicsSim fluid restart support. Failed
inserts roll back receipts and cursors. Retry the same identity after an uncertain
reply. First-use validation/order rejection creates no journal; an initial database
I/O failure can leave an empty file with no accepted event and is retryable.

Source JSON is limited to 64 MiB, the journal to a conservative 128 MiB admission
estimate, 256 events and 4096 named plans. Mapping entries and total sparse allocation
rows are bounded to 262144. These are trusted-local source tools; no remote input,
public upload or source-code execution interface is introduced.

## Verification and ownership

```sh
make test-surface-source-receiver
PHYSICS_SIM_SESSION_WORKER=/path/to/selected/source/worker make test-surface-source-receiver
```

The optional worker-dependent check compares a body-free manufactured transient
session while paused and after two accepted steps. Admission/planning leave all
solver status (excluding elapsed snapshot age), time and exported field bytes
unchanged. Other controls cover prescribed/zero/native communication, malformed
JSON, digest/provenance/calibration/geometry rejection, half-open substeps, partial
budgets, unequal grids, replay/conflicts, concurrency, capacity and injected SQL
rollback. Numeric conservation uses explicitly checked binary64 tolerances.

The normative validator in `scripts/surface_sources/growth_fire_v1.py` is an exact
pinned copy of GrowthSim's contract; `upstream.json` records source revision/hash.
Its hash and independent digest known answers are tested. Contract changes require
an explicit reviewed update. PhysicsSim owns admission policy, area/time allocation
and its journal. Shared core_space/core_units meanings are retained; core_math,
core_pack and core_memdb do not supply this source-specific Python interchange
adapter. A generic shared thermal-source module is deferred until actual receiving
physics has been proved. No shared API/version or adoption minimum changes here.

Next: select a bounded general-atmosphere receiving model and qualify passive
thermal energy/tracer transport before buoyancy. A body-free qualified transient
backend can be assessed for that purpose; masked obstacle startup is required only
if the chosen next case actually includes obstacle dynamics. Keep the earlier
CFD force investigation paused and separate from these source-contract milestones.

## Initial batch acceptance (2026-10-04)

The focused suite passed 15 tests with the selected independently built source
session worker, including native checkpoint tampering, signed-zero configuration
binding, concurrent replay and unchanged paused/completed solver artifacts. The
GrowthSim driver also passed two consecutive native three-tick intervals using
an explicit 16³ general-fluid grid policy and unequal receiving substeps. No native
C/C++ solver source or fluid session API changed in this batch. The driver proves
CLI interoperability and allocation, not attachment to a live solver grid.

Retained local evidence: PhysicsSim `build/surface-source-contract/` contains the
focused log, unchanged-solver hashes and verification manifest. GrowthSim
`build/physics-surface-contract/final-acceptance-v2/` contains original source
bundles/frames, admission/allocation receipts, SQLite journal and `acceptance.json`.
Build evidence is ignored local output; regression fixtures and these instructions
are committed source. This milestone is source-only; installed apps, existing MCP
capability, and thermal/fluid physics qualification do not change.


The next passive receiving batch is now implemented and qualified separately:
[passive atmosphere transport](passive_atmosphere.md). Admission/allocation remain
unchanged; their journal still does not record physically consumed source.
