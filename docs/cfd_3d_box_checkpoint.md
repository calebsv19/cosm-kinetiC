# Usable stationary-box CFD source checkpoint

Main Edit local source delivery, 2026-10-04. Authoritative checkout is
`/Users/calebsv/Desktop/CodeWork/_worktrees/physics_sim_main_edit`, branch
`codex/physics-sim-main-edit`. This finite batch prioritizes physical accuracy
and a usable scene workflow. The installed desktop and protected older worker
are unchanged. General wind-tunnel/transient-object CFD remains incomplete.

| Deliverable | Current boundary |
|---|---|
| Usable stationary-box scene/session workflow | Complete for the bounded steady-Stokes source mode, with provisional force estimates |
| One physical transient obstacle slice | Partial: exact implementation/validation specification delivered; no runtime capability implemented |
| Current cube reference investigation | Closed for this batch; original unmet accuracy gate and saved evidence retained |

The short durable checklist is [finite delivery status](cfd_3d_box_delivery.md).
The next specific implementation is [pressure-driven obstacle startup](cfd_3d_obstacle_startup_spec.md),
not another open-ended cube mesh/preconditioner investigation.

## What can be run now

`cfd_box_3d` authoring exposes `steady_box_duct` through the actual local scene
service, CLI and MCP catalog. It requires explicit physical `body_min_m` and
`body_max_m`; the geometry is retained through authoring, compilation, immutable
revision, request admission and native execution. Bounds cannot be combined with
the legacy cube's `center_x_m` or supplied under another template.

Example `scene_create` arguments:

```json
{
  "scene_id": "long-box",
  "template": "cfd_box_3d",
  "dimensions": [4, 2, 2],
  "channel": {
    "body_min_m": [1.25, 0.75, 0.75],
    "body_max_m": [2.75, 1.25, 1.25],
    "volume_flow_m3_s": 0.008
  }
}
```

Use its returned revision with one solve step, explicit fluid
`{"density_kg_m3":1,"dynamic_viscosity_pa_s":0.1}`, grid `[48,24,24]` or
`[96,48,48]`, dt=0.01 s and `numerical_memory_limit_mib=256`. Time remains zero:
dt is a required session parameter and does not turn this solve into a transient.
Stationary temporal comparisons are rejected.

The following developer commands reproduce authoring, run, retained inspection,
separate forces, complete export/native equation readback and a matched two-grid
comparison. Run them from the authoritative Main Edit checkout. They require
the existing clang/build dependencies. The active box runner and inspector
use Python standard-library tooling. Build the pinned reference environment
separately for independent-reference tests.
Each named output is immutable; choose a fresh name/output directory when repeating.

```bash
make BUILD_DIR=build/c3d-box/workflow-build CFD_BUILD_OPT=1 \
  SESSION_WORKER_BIN="$PWD/build/c3d-box/workflow-build/physics_sim_session_worker" \
  "$PWD/build/c3d-box/workflow-build/physics_sim_session_worker"

PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  python3 -B scripts/run_cfd_box_scene.py --name my-box-01

PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  python3 -B scripts/inspect_cfd_box_scene.py \
  --receipt data/experiments/c3d-box/scenes/my-box-01/receipt.json \
  --output data/experiments/c3d-box/inspection/my-box-01
```

Open the resulting `data/experiments/c3d-box/inspection/my-box-01/index.html` for labelled
velocity/pressure previews, SI probes, force components and resolution sensitivity.
The individual case JSON files link full XYZ loads, physical/numerical assessment
and field/worker hashes. Retained `channel_fields.json` contains every cell/face
with declared SI locations, solid mask/null solid pressure and separate upper-X
velocity. Paused/running worker sampling remains available; after terminal completion
use these retained fields, snapshots and reports. A new live sample after terminal
completion is intentionally unavailable. Inspection does not advance a solver.

Direct CLI/MCP operations continue to use `scripts/physics_sim_session.py`; point
`--worker` at the separately named worker above and choose a local `--root`.
`capabilities`, `scene_create`, `scene_validate`, `run_start`, `run_inspect`,
`run_result`, `run_assess` and spatial `run_compare` all include the new source mode.
The existing scene compiler and local supervision are reused; no untrusted upload
or remote-submission capability is introduced.

## Physics and numerical limits

The native `cfd_obstacle3d_box_init` API reuses the original compact masked
velocity/pressure Stokes solver. Its equations, B/-B^T, viscosity, pressure modes,
residual requirements and default force observers are unchanged. Y/Z duct lengths
remain 2 m because the original end-load observations use a 4 m² end area. Body
faces must lie exactly on grid planes, with at least two body cells and two fluid
cells between each body plane and the corresponding outer plane. No snapping.

All body/Y/Z wall planes are no-slip. Both X ends have natural vector-Laplacian
traction `mu du/dn - p n = -P n`; Pin is solved to enforce Q, Pout=0. Density,
viscosity and positive flow are explicit SI. Bulk Reynolds admission is
`rho*(Q/4)*max(body extent)/mu <= 0.1`. The example has bulk Re=0.03. This is a
confined creeping-flow model without inertial transport, moving/rotated bodies,
arbitrary STL, turbulence or validated wakes. The admission bound is not an
absolute force certificate.

Snapshots now use actual body bounds, characteristic length, projected YZ area,
all six face areas, correctly normalized drag coefficient and body-centred
downstream recovery locations. Box physical-momentum assessment checks the largest
XYZ residual, with a deliberate transverse-failure test. The legacy cube
geometry, defaults and numerical behavior remain intact.

The actual session limit stays 262144 cells, 4..256 per axis, with the original
explicit numerical owner budget. The example uses 256 MiB. Its source runner
declares 600 s per scene case and 1536 MiB sampled worker RSS; its native readback
has a 180 s allowance. Higher developer field probes separately admit up to
1048576 cells, 1 GiB owner and 1536 MiB sampled RSS. Those developer probes do not
expand the local session admission contract.

## Reproducible scene result

The final frozen packet is
`build/c3d-box/scenes/checkpoint-workflow/receipt.json`; the retained inspection is
`build/c3d-box/inspection/checkpoint/index.html`. Both resolutions pass native full
SI equation readback of every exported compact velocity and fluid pressure value.

| Diagnostic | [48,24,24] | [96,48,48] |
|---|---:|---:|
| Inlet pressure (Pa) | 0.01088429339 | 0.01110349419 |
| Pressure body force X (N) | 0.003535445667 | 0.003790217885 |
| Viscous body force X (N) | 0.006869748376 | 0.007077204627 |
| Physical dissipation (W) | 0.00008839412108 | 0.00008953465598 |
| Physical energy imbalance | 1.2864% | 0.5684% |
| Physical momentum imbalance | 1.0449% | 0.4944% |

The original full residual ≤1e-11, divergence <1e-8/s, flux <1e-9 and separate
discrete momentum/energy 1e-9 gates pass. Fine complete scaled SI residual is
6.8888e-14, divergence 3.3789e-13/s and flux error 8.8536e-13. Numerical owner
peak is 217864064 bytes, below 256 MiB. All actual final cost/RSS values are in
the packet; speed is not an accuracy selector.

Scene comparison uses the finer value as denominator: pressure force changes
6.7218%, viscous force 2.9313%, pressure drop 1.9742% and dissipation 1.2738%.
Thus this usable example passes equations and physical budgets, while its two
resolutions do **not** establish 1% force convergence. All source assessments
retain `reference_accuracy.status=not_established` and
`physical_accuracy_certified=false`. Transverse symmetric loads are at roundoff:
compare their absolute magnitudes, not relative percentages between tiny values.

## Broader measured stationary-body evidence

Nineteen completed developer fields cover long/short boxes, smaller/larger cubes,
a displaced box, multiple refinements, anisotropic narrow-gap cells and tunnel
length. Every completed field passes fresh full native SI readback. An independent
Python integration reconstructs pressure on each actual physical body plane from
the complete exported pressure array, multiplies actual areas and matches all
three optional pressure-trace components to 1e-12 N. This is observer consistency,
not an independently solved absolute force reference.

| Native refinement (coarse denominator) | Largest relevant result | Decision |
|---|---|---|
| Short box n64→n80 | Pressure force 0.8661%, viscous force 0.4621%, Pin 0.2359%, dissipation 0.2127% | Passes the declared 1% successive-grid screen |
| Long box n64→n80 | Pressure force 1.1270%; other three metrics <0.2% | Still fails 1% pressure-force screen |
| Large box n32→n64 | Pressure/viscous force changes 8.98%/10.06% | Underresolved |
| Large narrow-gap anisotropic pair | Pressure/viscous force changes 2.73%/2.90%; Pin/dissipation 1.84%/2.15% | Improved resolution; still fails 1% screen |

The narrow-gap fine field uses [64,128,128], preserving actual 1.5 m body and
0.25 m Y/Z gaps. Physical energy/momentum imbalances are 0.3581%/0.9559%, full SI
residual 5.3715e-14. These conservation passes do not remove its grid sensitivity.
Matched-cell-spacing L4→L6 tests translate the body and move both end planes away:
long-box pressure/viscous force changes 0.031995%/0.002896%; short-box
0.0006122%/0.00001856%. Both pass the prospective 1% body-force domain screen.
Total pressure drop/dissipation are not claimed length-invariant, since the longer
duct adds wall work.

Two original 600 s field attempts (`short-n80`, `large-gap-fine`) stopped at a
Krylov checkpoint and published no accepted field. They remain failures. A
separately declared diagnostic namespace allowed 1800 s with exactly matched
geometry/material/equations/residual/memory/cell caps, retaining the failed parent
identities. Both diagnostic repeats completed and passed full readback. This
does not reinterpret the earlier failures or make timing an accuracy criterion.
See `build/c3d-box-accuracy/selection-contract.json` and both immutable run roots.

Backend ordinary/sanitized tests pass: bitwise cube U/P and load parity; actual
box Y-reflection and Y/Z-exchange fields/forces/scalars (~1e-14); cancellation
retains accepted fields; thirty finite/alignment/margin/Re/exact-memory rejection
controls; and nine complete-field material/flow/density scaling cases (maximum
3.9569e-13 relative error). Native session U/P are bitwise identical to direct
box execution. Actual MCP discovery/scene/step/mask/pressure export, revision
conflicts, admission and low-memory cleanup pass. Existing cube and Cartesian
agent regressions plus ordinary/sanitized native sessions pass. Initial example
protocol/terminal-sampling attempts and initial test expectation failures remain
in their logs/receipts; their corrected passing packets are distinct.

Focused developer targets:

```bash
make test-cfd-obstacle3d-box test-cfd-obstacle3d-box-sanitize
make test-cfd-obstacle3d-box-material test-cfd-obstacle3d-box-material-sanitize
make test-cfd-3d-box-session test-cfd-3d-box-session-sanitize
make BUILD_DIR=build/c3d-box/workflow-build CFD_BUILD_OPT=1 \
  SESSION_WORKER_BIN="$PWD/build/c3d-box/workflow-build/physics_sim_session_worker" \
  test-agent-box3d
```

Assessment/evidence: `build/c3d-box/physical-assessment.json`,
`build/c3d-box/checkpoint-audit.json`, frozen field receipts and source worker
workflow packet. Existing solver/allocator/frozen supervision are reused;
app-specific box policy and validation remain local. Shared core/kit APIs and
versions are unchanged. No commit, package, install or default force switch.

## Cube reference investigation: current stopping point

The retained independent paired cube reference has raw physical surface/reaction
mismatches 1.1951% (L4) and 1.1996% (L8). A thinner side-normal L4 trial reaches
1.1343%, but fails the original raw 1% gate and its prospectively required 10%
improvement for a paired run. The finer edge trial worsens to 1.4114% and stays
rejected. No new cube mesh/preconditioner experiment is needed for this workflow,
and none is run in this finite delivery window.

These results permit numerical/reference diagnostics, provisional body-force
comparisons and measured grid/domain sensitivity. They block a certified
sub-1% cube-force claim and use of this reference as an unqualified force oracle.
The mismatch is not a proven ±1.2% absolute-error bound. It does not block adding
a separately labelled stationary box workflow or the next pressure-driven
transient-Stokes implementation. All original gates and saved successes/failures
remain intact; the investigation stops here for this batch.

## Build and evidence lifecycle repair (2026-10-06)

Historical numerical values above describe the earlier checkpoint; lost historical
fields cannot be inferred present from these descriptions. Fresh receipts and
remaining qualification gaps are tracked in [repair status](cfd_lifecycle_repair_status.md).
See [portable retained bundles and cleanup](cfd_evidence_lifecycle.md) before new runs.

## Current contract evidence location

The six native box contract/material/session targets now retain each attempt in
`EXPERIMENT_DIR/contract-proofs/<target>-<uuid>/`. Printed capsule paths contain
`compile.stdout`, `compile.stderr`, `run.stdout`, `run.stderr` and sealed readback
metadata. Earlier build-tree paths in this checkpoint describe historical
evidence; new runs preserve them. See [native contract proof lifecycle](native_contract_proof_lifecycle.md).
