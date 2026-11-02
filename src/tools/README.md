`physics_sim_inert_io.c/.h` compose the existing gas/object exchange with an optional closed native JSON checkpoint. Worker defaults omit object state; explicit object requests share gas steps and clocks. See `docs/fire_fluid_inert_checkpoint.md`.

`physics_sim_source_object_probe.c` is a standalone controlled-heater transport diagnostic; source/off, exchange/off and wind/off controls reuse the native flow and existing inert exchange. It adds no worker/app mode. See `docs/fire_fluid_source_object.md`.

- `physics_sim_prescribed_transport_probe.c` — exact-reference thin-column pulse

## 2026-10-22 — Explicit on/off age intervals repair bounded shutoff/restart histories

New opt-in source_age_intervals module retains source on/off intervals on the
existing scalar clock, including nonzero youngest age after shutoff and positive
interval sums after restart. Existing FCT budgets, incoming-first staging,
actual bounds, grid, source delivery/clocks and10% gate unchanged. Frozen pulse
and restart requested/admitted flux pass56 comparisons. Same four evolving
histories pass48 selected gates;173continuous-control checks and36heater-neighbor
checks also pass. Pulse first-cell at0.00625s improves-17.659% to-0.0001824%;
restart endpoint first-cell-0.0002236%. Tiny discrepancies are numerical
comparisons within unchanged0.5% reference qualification, not precision claims.
14tests strict/ASan-UBSan, full new-history sanitizer parity, delayed-state parity
and five old-route trace comparisons pass. Source transitions/capacity/invalid
ages reject atomically. New route2 is explicit; no default or app adoption.
Qualified fixed-footprint stationary1-D unit on/off heating through0.025s;
next evolving matched transport test before shear selection. No2-D/feedback.
Existing Atlas experiment remains active; no new topology/adoption claim.
Evidence: `_private_workspace_artifacts/fire-fluid-age-interval-repair-20261022/RESULT.md`.

---


## 2026-10-22 — Source-age closure fails shutoff; frozen transport tests pass locally

Bounded same-grid/history comparison: delayed activation reproduces the
continuous case exactly after time shift. Safe generic dispatch after shutoff
has first-cell-17.659% at0.00625s despite passing late endpoint. Restart inherits
that pre-restart failure; no separate new post-restart failure proven. On exact
frozen pulse means, continuous-age closure requested flux is up to99.970% low,
with14/14sampled diffusion faces failing10%; near-heater flux is62.513% high.
Ages are0.003125..0.0046875s but closure infers0..0.000534905s. Thus history
information is needed, not a grid refinement. Frozen continuous transported
shapes at+/-0.025m/s pass local10%: diffusion max4.919%, advection face max2.582%.
This is not an evolving-flow qualification. No general reconstruction selected.
Five histories/eight frozen cases match strict/ASan-UBSan outputs; three affected
fixtures pass both builds, numerical headers unchanged. Source-off metadata
refusal is atomic; forced unsupported closure is labeled diagnostic only.
Next finite-age interval/multi-interval reconstruction, then same pulse/restart
histories before full transport selection. No2-D/feedback/default adoption.
Evidence: `_private_workspace_artifacts/fire-fluid-history-transport-20261022/RESULT.md`.

---


## 2026-10-22 — Source-age reconstruction and incoming/outgoing staging pass the bounded 1-D history

Opt-in source-age profile4 repairs finite-duration heater shape and processes
incoming face transfers before outgoing ones, refreshing unchanged FCT budgets
and actual-cell guards. Grid, clocks, source, bounds and gates unchanged.
All202 reference-qualified measurements across the retained1-D history pass
10%; worst error0.5134%. Band errors at0.0015625/0.0046875/0.025s are
+0.08589%/+0.11966%/+0.00822%; endpoint first-cell+0.05538%. First-interval
requested/admitted flux, initially cold-cell heat, perturbation transport and
invalid-input refusal pass.13 tests strict/ASan-UBSan, full history sanitizer
parity and old profiles0..3 exact trace parity pass. Qualified only stationary
continuously powered unit rectangular-source1-D diffusion through0.025s;
no source-shutoff/advection/2-D/default/feedback claim. Next qualify closure
under changed source histories and transport before prescribed-shear use.
Existing Atlas experiment retained. Minor release recommended; no version edit.
Evidence: `_private_workspace_artifacts/fire-fluid-source-age-staging-20261022/RESULT.md`.

---


## 2026-10-22 — Whole-interval front-flux candidate rejected; two residual mechanisms isolated

Whole-interval diffusion of a fitted positive profile passes24 analytic flux
cases (worst relative error5.15e-11), but unchanged256-cell history still fails
10%: normal-band errors+41.0754% at0.0015625s,+14.1843% at0.0046875s,
-12.6360% at0.025s; endpoint first-cell-15.8200%. First-interval face63/64
requests already overestimate finite-source heat by40.94%/33.51%. Farther cold
face60 requests within0.035%, but combined budget/application blocks nearly
all of it; incoming heat is unavailable to outgoing whole-interval transfer.
Face58 additionally misses representable arrival from initially zero means.
Candidate remains private and rejected; solver source/defaults/limiter,
physical clocks and gates unchanged. Strict/sanitizer frozen/history parity,
bounds and six affected regression executions pass. No2-D or feedback run.
Next finite-duration source-age reconstruction plus consistently staged
incoming/outgoing flux, retaining budgets. Supporting existing Atlas lane.
Evidence: `_private_workspace_artifacts/fire-fluid-spacetime-front-20261022/RESULT.md`.

---

  transport through the ordinary native operator. Arguments are axial cells,
  dt, conductivity, axis and half-width; optional `mc` selects the experimental
  scalar limiter. At most 4×4×256 cells; no cubic-grid capacity extension.
  `fire_fluid_transport_report.py` evaluates cell-average translation/diffusion.
  `fire_fluid_trace_diagnosis.py` diagnoses retained timing/shape without changing
  the acceptance gate. Build with `prescribed-transport-probe` or
  `prescribed-transport-sanitize` in a fresh build root.
- The boxed probe accepts optional observation steps then `mc`. SIGTERM/SIGINT
  save the last accepted field and closure with incomplete status; use
  `scripts/fire_fluid_box_forcing_run.py` for bounded soft-stop supervision.
  The resulting field is diagnostic, not a resumable coupled checkpoint.

- `physics_sim_box_forcing_probe.c` — bounded current boxed-plume qualification
  on 32³/64³ only, with fixed 2.5 ms flow and 1.25 ms scalar timing. Samples the
  full lower box face over a fixed 1/32 m gas slab, conservatively restricted to
  common 8×8 face tiles. Default observations are 25 ms; optional final argument
  `2` or `5` selects 5 ms or 12.5 ms observation timing without changing physics.
  Build with `box-forcing-probe`. `scripts/fire_fluid_box_forcing_report.py`
  verifies input/output hashes, native closure, physical support, the unchanged
  10% gate and a separate reciprocal frozen-field oracle. It reports prescribed
  heat demand separately from actual debited gas energy. No larger-grid admission
  or live coupled host is provided.
- `physics_sim_heated_face_sample` and `physics_sim_heated_face_exchange_depth`
  use identical aligned physical support and uniform volume/conductance weights.
  Depth must span whole gas cells and stay in-domain; unmatched depths refuse.
  The old one-cell exchange wrapper is preserved. This synthetic distributed
  Robin law is not a resolved wall-gradient or calibrated convection model.

- `physics_sim_heated_face.c/.h` — non-burning, synthetic-coefficient reciprocal
  gas/face exchange, with atomic refusal and compensated weak-signal temperature.
  `physics_sim_heated_face_probe.c` uses uniform initial gas or retained boxed-plume
  energy as a frozen-transport thermal experiment. Build/test routes live in
  `make/rules-heated-face.mk`; independent matrix-exponential qualification is
  `scripts/fire_fluid_heated_face_qualification.py`. No ordinary worker or joint
  checkpoint linkage. See `docs/fire_fluid_surface.md` for the physical stop boundary.

# `src/tools/`

Command-line and diagnostic tools live here.

- `cli/physics_sim_headless.c` - direct retained-scene, Water, Wind, volume,
  and render-frame headless runner.
- `cli/physics_sim_job_runner.c` - detached submit/status/cancel runner and
  job-status surface.
- `cli/physics_trace_tool.c`, `cli/vf2d_pack_tool.c`, and
  `cli/vf2d_dataset_tool.c` - trace, pack, and dataset export tooling.
- `cli/runtime_scene_emitter_diag_tool.c` - runtime-scene, runtime mesh,
  emitter, footprint, and Wind projected-area diagnostic summaries.
- `cli/shape_*` plus `ShapeLib/` - shape import, flattening, validation, and
  ShapeAsset conversion tools.
- `physics_sim_flow_time_probe.c` - contained 64-cubed evolving plume
  qualification with flow steps of 5, 2.5 or 1.25 ms and fixed 1.25 ms heat
  and smoke steps. Build with `open-flow-time-probe` in a fresh `BUILD_DIR`;
  the compile profile is separate from ordinary worker admission. Input is
  the frozen 32-window native source fixture and output is a new binary field
  path. `scripts/fire_fluid_flow_time_report.py` reads the terminal three-case
  replay. This tool establishes temporal sensitivity, not spatial convergence
  or material/surface qualification.

- The same probe has a separate `open-flow-spatial-probe` profile for paired
  independent 64/128-cubed flow at 5 ms, with fixed 1.25 ms heat/smoke timing.
  `physics_sim_flow_source.c` embeds the original fine receiver's compact lower
  source support into the full 128-cubed domain. Fine input is **not** parent/8:
  that changes the source shape near the original Fire nodal boundaries.
  At 64 the existing full source input is copied exactly. Both native input
  layouts happen to contain 524,288 doubles per window: two full 64-cubed fields
  versus two 128x128x16 support fields. The qualified replay binds their distinct
  hashes and checks the reconstructed full fine source against the prior fixture.
  `scripts/fire_fluid_flow_spatial_report.py` uses the bundled NumPy interpreter
  for finite-volume scalar restriction, normal-face area velocity restriction,
  native wind probes and source-adjacent localization. The 10% physical heat
  region stays unchanged; fine-grid temporal and surface qualification are
  separate boundaries.

- `physics_sim_buoyancy_projection_probe.c` - one smooth manufactured thermal
  projection impulse with independent continuum and discrete eigenfunction
  references. Build `buoyancy-projection-probe`; grids 8/16/32/64/128 are
  qualification-only. `test-buoyancy-projection-sanitize` covers 8/16. Pressure
  and MAC face-area means are compared at the original mixed pressure
  boundaries. This checks forcing/projection consistency, not nonlinear plume,
  finite-time wall viscosity, surface heating or physical calibration.

- `physics_sim_matched_original_probe.c` - corrected matched-velocity heat
  attribution with original source mapping at both grids and fixed 1.25 ms
  scalar timing. `open-matched-original-probe` takes the full 64 source,
  original fine compact support and a fresh output directory. The 64 driver
  evolves accepted flow, a 64 heat control must match every step exactly, and
  the 128 heat carrier follows flux-preserving prolonged driver velocity.
  This does not independently solve fine momentum. It supersedes the earlier
  parent/8-source diagnostic for original-source attribution. Analyze its
  terminal replay with `scripts/fire_fluid_matched_original_report.py` and
  the separately completed evolving-flow pair. Do not infer additive shares
  from the difference norms.

- `physics_sim_prescribed_flow.c` and `physics_sim_prescribed_flow_probe.c` -
  online 64-cubed accepted driver and independently evolving 128-cubed flow
  under the driver's **old-time** thermal cell averages. Build
  `open-prescribed-flow-probe` in a fresh `BUILD_DIR`; arguments are the original
  full 64 source, original fine compact source and fresh output directory.
  The helper temporarily supplies extensive thermal averages only to momentum,
  restores accepted heat before scalar advancement, and preserves all accepted
  commit/refusal paths. Native/sanitizer controls cover exact ordinary evolution,
  conservative temperature mapping, no scalar injection and refusal restoration.
  The diagnostic shares 5 ms flow and 1.25 ms scalar timing, emits final fields
  and the last old-time forcing samples, and stores no large histories. Analyze
  the completed replay with `scripts/fire_fluid_prescribed_flow_report.py`.
  Fine heat/smoke retain the original source mapping and their own transport;
  coarse forcing is piecewise constant in temperature. This is externally forced
  attribution, not a newly qualified fully coupled solver or worker capability.

- `physics_sim_thermal_transfer.c` and `physics_sim_reconstructed_flow_probe.c` —
  qualification-only conservative bounded MC reconstruction of the coarse
  old-time thermal averages. Build `open-reconstructed-flow-probe`; use the
  same three arguments as the piecewise-constant probe. Eight child offsets
  preserve each parent mean; one multidimensional limiter and final floating-
  point bounds enforcement prevent overshoot. Domain-edge slopes are zero.
  This changes the prescribed momentum input, not either grid's heat source.
  `test-thermal-transfer-native` and `test-thermal-transfer-sanitize` check
  constants, affine interiors, sharp/graded support, smooth refinement and
  admission/refusal closure. `scripts/fire_fluid_reconstructed_flow_report.py`
  compares terminal transfer and separate fine timing with frozen controls.

- `open-fine-flow-time-probe` compiles the existing flow probe in a separate
  128-cubed timing profile. Arguments: `128 <0.005|0.0025|0.00125> 0
  <original-fine-support.bin> <new-fields.bin>`. Its own accepted heat drives
  momentum; reconstructed forcing is absent. Original source windows and
  1.25 ms scalar timing remain fixed. Ordinary worker admission is unchanged.

- `open-vertical-flow-probe` uses the same accepted flow probe with a contained
  `PHYSICS_SIM_OPEN_VERTICAL_REFERENCE` capacity profile. The reference is
  128x128x256 at 5 ms flow and 1.25 ms scalar timing, with independently evolving
  momentum and heat. `physics_sim_vertical_source.c` splits each original
  128-cubed extensive source sample equally into two vertical children; XY
  samples and lower-1/8 physical support stay unchanged. Arguments:
  `128x128x256 .005 0 <original-fine-support.bin> <new-fields.bin>`.
  An optional final `1` requests a one-window pilot and requires a one-window
  source file. Full qualification uses all 32 original windows. The contained
  profile admits at most 4,194,304 cells, a 4 GiB tracked allocation budget and
  12 billion scalar work cells; ordinary worker and prior full-grid admission
  remain unchanged. `test-vertical-source-native` / `test-vertical-source-sanitize`
  verify conservative splitting, old isotropic mapping parity, refusal and
  anisotropic flow accounting. `scripts/fire_fluid_vertical_reference_report.py`
  compares terminal fields against the frozen 128-cubed baseline. This is
  directional refinement only, not full three-axis convergence or a solver repair.

- `open-momentum-diagnostic-probe` enables observers only in the contained
  vertical-reference profile. It keeps the original solver math and reports
  predictor finiteness, momentum CFL, pressure-cache/allocation/solve, CG
  breakdown and projected divergence separately. It saves the last accepted
  state, rejected candidate scratch, rejected-step source and JSON observations
  on refusal, using the ordinary probe output argument as the capture prefix.
  `physics_sim_momentum_capture.c` serializes a trusted-local same-ABI state:
  twelve native uint64 values, twenty-six native doubles, then velocity,
  pressure, heat, smoke and four boundary-flux receipt blocks. Pointers and
  cached matrices are excluded. Parameters/time/counters are preserved; matrices
  are rebuilt. `open-momentum-replay` loads this state and retries one step:
  `<accepted.bin> <source.bin> <new-prefix>`. This is a qualification helper,
  not an ordinary worker checkpoint protocol or cross-platform format.
  Native/sanitizer tests prove exact small-case save/load/continued-step parity
  and refused-state preservation, plus distinct CFL/cache/CG/divergence checks.
  Candidate pressure/RHS are phase-invalid when pressure was not reached;
  solver residuals are the last reported true residual and may precede an early
  terminal CG breakdown. `scripts/fire_fluid_momentum_readback.py` independently
  checks accepted accounting/divergence, the recorded worst-face CFL, valid
  candidate pressure/divergence, and the original rejected-step source mapping.
  The 10% spatial heat gate stays unchanged; a captured refusal is not a spatial pass.

- `open-matched-vertical-probe` is a separate 128x128x256 qualification profile
  at 2.5 ms flow and fixed 1.25 ms scalar timing. Original source windows divide
  into forty flow inputs; ordinary admission and the earlier 5 ms vertical
  profile remain unchanged. Use `128x128x256 .0025 0 <fine-support.bin>
  <new-fields.bin>`. `open-momentum-replay` also accepts an optional final
  `half-flow` for two 2.5 ms steps from a saved 5 ms state. It halves source,
  rebases the logical step index while preserving physical time/fields/receipts,
  and records retiming.json. This local mixed-history test is distinct from a
  full new reference started from zero. `physics_sim_half_flow.c` and its
  native/sanitizer tests cover prevalidation, source closure, clock/cycle
  partition and conservation. `scripts/fire_fluid_matched_vertical_report.py`
  binds the old completed 64/128 2.5 ms baselines and the new full vertical case,
  with finite-volume scalar and face-area velocity restriction and native probes.
  The 10% heat region/gate remains unchanged; no surface or burning-object claim.

- `open-matched-horizontal-probe` uses a separate capacity profile for the
  first horizontal X refinement: 256x128x256 at 2.5 ms flow and 1.25 ms scalar
  timing. `physics_sim_horizontal_source.c` splits original compact source
  into X/Z children; X sums recover the accepted vertical source exactly.
  Arguments: `256x128x256 .0025 0 <fine-support.bin> <new-fields.bin>`.
  Tracked memory is capped at 8 GiB, scalar work at 22 billion and cells at
  8,388,608. Native/sanitizer tests cover conservative source restriction,
  refusal preservation, timing and accounting. The horizontal report restricts
  extensive scalars by sum and tangential face areas by average at fixed Z.
  Y refinement, surface heating and burning faces remain separate gates.

Tools may call app/import/export contracts, but they should keep product
runtime policy in the owning source lane. Generated outputs belong under
ignored `tmp/`, `build/`, `export/`, or caller-selected output roots.

### Same-operator boxed transport isolation

`make BUILD_DIR=build/<fresh> matched-box-probe test-matched-box-native
 test-matched-box-sanitize test-matched-heat-small source-edge-probe` builds
small local diagnostics. Invoke `matched-box-probe source32.bin source64.bin
output.bin mc` (or `minmod`) with the original 32-window source carriers.
It runs one native 32³ flow and same-operator 32³/64³ scalar replays at
2.5 ms flow / 1.25 ms scalar timing, with aligned insulated boxes. Every coarse
replay step must match native heat bitwise. Prolongation preserves coarse-face
flux and divergence and refuses nonzero blocked faces. SIGTERM preserves the
accepted heat fields and terminal closure (exit 7 for incomplete). Use a bounded
supervisor; fields are diagnostics, not resumable coupled checkpoints.

`source-edge-probe source32.bin source64.bin` performs only two steps with
zero velocity, conductivity zero/0.026 and both scalar reconstructions.
`fire_fluid_matched_box_report.py <evidence-root>` requires complete hash-bound
inputs, native closure, fixed 10% gates and independent final-field sampling.
The prescribed flow result does not establish fine momentum or physical wall
convection. These standalone mains are excluded from app source discovery.

### Sharp-source diffusion benchmark

Build `sharp-diffusion-probe` or `sharp-diffusion-sanitize` with a fresh
`BUILD_DIR`. Arguments: `32|64|128|256 length_m steps end_s [rk2]`, with
length1/0.0625/0.03125 m, steps1–8192 and end0.0025/3.2 s. The source occupies
the bottom1/8 of the1 m column or half a zoom column, at1 K/s. Zero velocity,
insulated diffusion endpoints, fixed physical conductivity. Actual work counts
must show no hidden adaptive scalar substeps. No fine momentum or wall model.

`scripts/fire_fluid_sharp_diffusion.py --probe <binary> --output <fresh-root>
[--rk2]` runs the bounded20-case matrix and compares cell averages against
independent continuum and exact grid-time references. The additional zoom uses
`256 .03125 128 .0025 rk2` and512 steps. Use
`tests/test_sharp_diffusion_reference.py` for independent reference checks.
Each native case has a45 s wall budget. No criterion is normalized by ambient
temperature; the outside-source thermal-excess profile retains the10% gate.

### Sharp-source advection–diffusion and leading-tail qualification

`sharp-transport-probe` evolves an interior0.125–0.25 m source at1 K/s with
constant0.05 m/s flow and SSPRK2 scalar timing. Its CLI is
`n length steps conductivity mc|minmod`; ordinary build n32/64/128/256,
length1/0.5 m, steps2560/5120, conductivity0/0.026. Observations every0.025 s
use fixed slabs11/32–12/32 m and14/32–15/32 m, independently of resolution.
The sharp source is shifted into the interior to isolate transport from ground
conditions and observe arrival within3.2 s. No obstacle or reciprocal wall model.

`fire_fluid_sharp_transport.py --probe <binary> --output <fresh-root>` runs18
bounded cases with45 s per-case limits and preserved partial output on timeout.
It uses an independent time-integrated translated heat kernel, with exact
zero-diffusion translation and doubled-quadrature checks. Main profile, near
history, early prefront history and far-tail history are separate10% checks;
zero reference values retain absolute leakage rather than an invented ratio.

`scalar-flux-probe` takes `n length exact-profile.bin mc|minmod` and uses one
1-microsecond Euler step on exact supplied averages to measure instantaneous
spatial flux, independently of trajectory timing. Run the Python report with
`--flux-check` and that probe for10 small exact-profile flux checks.

`sharp-transport-thin-probe` is a separate compile-time qualifier permitting
4×4×512 and4×4×1024 shapes only beyond ordinary axis limits. Newly admitted
shapes have at most16,384 cells. `test-thin-transport-capacity` verifies ordinary
refusal and narrow qualifier admission. Ordinary app/worker limits are unchanged.
The targeted refinement commands are `512 .5 2560 .026 mc` and
`1024 .5 2560 .026 mc`, supervised with90 s limits; native resource bounds remain
16 MiB and200 million scalar work cells. No cubic capacity expansion.

### Selected finest-column timing diagnostic

Build `make BUILD_DIR=build/<fresh-task-root> sharp-transport-thin-probe`, then
use a Python with NumPy:

```sh
python3 -B scripts/fire_fluid_sharp_transport.py --case finest-half-step \
  --probe build/<fresh-task-root>/heated-face/sharp-transport-thin-probe \
  --output <new-evidence-directory>
```

Runs only 4×4×1024 cells, length 0.5 m, 5120 SSPRK2 steps, dt 0.000625 s,
physical conductivity 0.026, MC reconstruction and the existing source/supports.
Native runtime cap 180 s; gate remains 0.1. Default matrix and `--flux-check`
remain separate selections. Completed observations flush for timeout diagnosis.
On 2026-10-16 all four gates pass; early history is 9.956535%, a narrow margin.
The prior 90 s timeout remains retained. This does not qualify boxed coupling.

### Prescribed-flow Thermal receiver benchmark

`physics_sim_thermal_transport_probe.c` links the existing native gas solver and
Thermal Main Edit's receiver to shared core_thermal0.3.2 in an optional development
qualifier. No ordinary app/worker dependency, solver default or shared pin changes.
`physics_sim_thermal_transport_reference.c` is an independent central-FV /
Crank-Nicolson linear reference with a direct tridiagonal/Schur solve; it calls
neither numerical owner. Reference mesh/time refinement and the zero-exchange
heat-kernel comparison precede native acceptance.

From PhysicsSim Main Edit, select fresh output roots:

```sh
python3 -B scripts/build_thermal_transport.py --output build/NEW-coupled \
  --thermal ../thermal_sim_main_edit --shared ../../shared
python3 -B scripts/fire_fluid_thermal_transport.py --build build/NEW-coupled \
  --output /ABS/NEW-evidence
```

The analysis Python needs NumPy. Each native full run has a300s wall cap; reference
runs have30s caps (60s for the final32768cell oracle), preserve partial stdout and record terminal receipts.
Two native cases use4×4×1024cells,dt1/1600s,5120steps,40/20tick windows. Domain is
0.25×0.25×0.5m,source z[0.125,0.25]at1K/s,u0.05m/s,k0.026,rhoCp1206.
The full-XY exchange slab z[0.34375,0.375]has area0.0625m² and capacity2.35546875J/K.
It is an authored nonlocal Robin exchange region,not an impermeable wall or box.
Its temporary surface uses50J/K and h10W/(m²K); the native two-cell solid uses
50J/K per cell and2W/Kinternal conductance. Native receiving-cell rise is retained
when initializing the temporary surface; only signed Q is adopted into the solid.

The host stages disjoint gas and solid candidates and checks source/boundary/interface
and solid ledgers before publication. Reports keep gas arrival,each solid cell and
Q histories separate. Zero-reference quantities are not assigned fabricated relative
passes. Invalid-packet and post-solid-stage refusal controls preserve accepted state.
The shared slab/box pair-transfer kernel retains the existing box orientation/support
behavior. `tests/cfd_heated_slab_test.c` checks the new region,analytic transfer,
zero exchange,invalid alignment/law atomicity and unchanged exterior cells.

This optional source-linked qualifier is not a Desktop,worker,full-fire or resolved
wall-convection integration. Current evidence is under
`_private_workspace_artifacts/fire-fluid-thermal-transport-20261016/`.

### Stationary wall, matched prescribed shear

```sh
python3 -B scripts/build_thermal_transport.py --wall --output build/NEW-wall \
  --thermal ../thermal_sim_main_edit --shared ../../shared
python3 -B scripts/fire_fluid_thermal_wall.py --build build/NEW-wall \
  --output /ABS/NEW-wall-evidence
```

RequiresNumPy for analysis. Optionalwall branch compiles the native operator via
`physics_sim_wall_prescribed.c`,holding tangential shear fixed and normalflowzero;
ordinarymomentum/workerbuilds are unchanged. It runs4×4×128/256nativegroundgrids,
fixedscalarstep1/3200s,5120flow/solidsteps,6.25/3.125msexchange windows. Contact
conductance includes gas/solidhalf-cell resistances and authoredinterfaceh10.
Independentnormal-diffusion/reference refinement precedes three90scapped native
runs. Uniform along-wall heating means this qualifies normaldiffusion/contact,
not advection of a thermalpatch or boxcorners. Historicalreportkeytail_k here
means the outergasband; it is not the previous benchmark's faradvective tail.
The defaultnon-wall build route and previousslab smoke remain qualified.

### Along-wall advected pulse

```sh
python3 -B scripts/build_thermal_transport.py --patch --output build/NEW-patch \
  --thermal ../thermal_sim_main_edit --shared ../../shared
python3 -B scripts/fire_fluid_thermal_patch.py --build build/NEW-patch \
  --output /ABS/NEW-patch-evidence
```

Requires NumPy. Separate patch probe/reference files preserve the stationary-wall
and original slab fixtures. The native periodic domain is 0.25 × 0.25 × 0.0625 m;
a 1 K/s source occupies x[1/32,1/16], z[1/64,1/32] for the first 0.4 s.
Prescribed u_x=0.05 z/0.0625 carries the pulse past fixed observation volumes.
Scalar dt remains 1/3200 s, flow/solid dt 1/1600 s, duration 3.2 s.
The 32/64 along-wall and 64/128 normal resolutions use four invariant y cells.
Five native cases each have a 180 s cap; independent reference runs have 120 s caps.

The independent Fourier-x / FV-z / Crank–Nicolson reference calls neither native
solver. Mode, normal-grid and time refinement must each change all measured
histories by less than 0.5% before native comparisons. Response gates remain 10%,
including early histories through 1.5 s; errors are summed absolute history error
divided by summed absolute reference, with no fabricated ratio for zero solid heat.
The no-transfer controls isolate gas transport. Exact accepted Q packets also drive
an analytic two-capacity solid replay with constant power over each exchange window.

The wall solid is two through-thickness cells, each uniform over the entire face;
it does not resolve a local solid hotspot or lateral solid conduction. This is
prescribed flow, not momentum/buoyancy feedback or a box/corner qualification.
Evidence: `_private_workspace_artifacts/fire-fluid-thermal-patch-20261017/RESULT.md`.

### Local wall strips and independent solid columns

```sh
python3 -B scripts/build_thermal_transport.py --columns --output build/NEW-columns \
  --thermal ../thermal_sim_main_edit --shared ../../shared
python3 -B scripts/fire_fluid_thermal_columns.py --edge-refinement \
  --build build/NEW-columns --output /ABS/NEW-columns-evidence
```

The column probe/reference are separate from the whole-face patch fixture.
Each gas x-strip maps to one geometry-bound two-cell receiver, with area,
capacity and internal conductance scaled by 1/nx. `physics_sim_heated_columns_exchange`
reuses the exact pair kernel with strided bottom-layer indices and batch rollback.
The host publishes only after every column and gas candidate accepts. Wrong-column
packets, late tampering and post-solid refusals leave all accepted participants intact.
No lateral solid conduction is included. The Fourier reference couples every mode
to both depth temperatures. The eight fixed observation patches preserve physical
sampling across the 64/128-column grids.

Default matrix: 64×4×64 / 3.125 ms, 64×4×128 / 3.125 and 6.25 ms. Optional
`--edge-refinement` adds 128×4×64 / 3.125 ms, retaining a 32,768-cell maximum.
Each native run has a 180 s cap; each reference has a 120 s cap. Same pulse,
physical coefficients, scalar timing and 10% gate as the previous patch fixture.
Primary hotspot/combined-history gates do not imply all local patches pass:
`local_patch_audit` reports every patch's error, absolute signal, individual
reference convergence and a true/false/null acceptance result. Null means the
reference is unqualified, never a pass. Current result is partial: main hotspots
pass but weak edge/wrap tails remain outside 10%; no tolerance waiver.
Evidence: `_private_workspace_artifacts/fire-fluid-thermal-columns-20261017/RESULT.md`.

### Frozen reference-field edge diagnostics

```sh
python3 -B scripts/build_thermal_edge.py --output build/NEW-edge
python3 -B scripts/fire_fluid_thermal_edge.py --build build/NEW-edge \
  --output /ABS/NEW-edge-evidence
```

Requires NumPy. The separate edge reference wraps the column reference's optional
snapshot hook; ordinary history output and evolution remain unchanged. It exports
1.5/3.2 s reference volume averages, analytic Fourier x derivatives and face values.
The native probe measures the existing scalar reconstruction/flux, validates its
extraction against actual scalar_euler column balances, then checks wall sampling
and frozen pair exchange separately. MC/minmod and two exchange durations are
controls. No momentum evolution or coupled trajectory is performed. The 32 native
calls each have a 15 s cap; four reference runs each have a 120 s cap.

Response limit remains 10%. Per-quantity reference refinement must be <0.5%;
unqualified measurements get null gates. No aggregate pass overrides a local
failure. Current evidence isolates spatial gas-flux failures on reference input,
with exchange-kernel error ≤0.0000041%. Whole-trajectory acceptance remains open.
See `_private_workspace_artifacts/fire-fluid-thermal-edge-20261017/RESULT.md`.

### Sharp-front reconstruction isolation (experimental)

`physics_sim_front_arrival_probe.c` runs a 1D periodic initial hot patch with
SSPRK2 and the same face reconstruction as the optional heated-wall candidate.
It separates face-value and diffusion-gradient fallback effects; it is not an
application default or a coupled Fire/Fluid acceptance test.

Build with `python3 scripts/build_front_arrival.py --output <fresh-build-root>`.
With NumPy available, run `python3 scripts/fire_fluid_front_arrival.py --build
<build-root> --output <fresh-result-root>`. The companion
`fire_fluid_front_diagnosis.py --build <build-root> --runs <result-root> --output
<fresh-diagnosis-root>` adds four 256-cell spatial controls and native parity.
Output paths must be fresh. Builds never clean existing output.

The reference is a periodic Gaussian heat kernel integrated over matching cells,
with an exact translated top-hat for pure advection. The 10% history gate and
0.5% reference-refinement requirement remain unchanged; physical positivity and
maximum-temperature bounds are reported separately. Nonnegative conservative
transport does not imply a maximum principle: the retained experimental
candidate overshoots the initial peak in the zero-diffusion control. Neither
simple fallback clears the sharp-front qualification. Keep both confined to
this diagnostic; do not adopt them as general scalar transport.

### Conservative cell-update correction (experimental)

`physics_sim_scalar_fct.h` adds a source-free, periodic 1D, constant-velocity
flux-corrected Euler stage. A monotone upwind/central-diffusion low-order update
provides the starting state; paired correction fluxes share the minimum of the
receiving and donating cells' available budgets. Global old-state lower/upper
bounds are enforced at each Euler stage, with SSPRK2 convex averaging. This is
not a multidimensional, variable-velocity or sourced application solver.

Build with the same `build_front_arrival.py` fresh-output route. Run
`python3 scripts/fire_fluid_front_fct.py --build <build-root> --output
<fresh-result-root>` to compare mode 4 (raw fourth-order target) and mode 5
(retained face-bounded target). The builder also produces `fct-test` and
`fct-test-sanitize` for arbitrary-profile and near-CFL bound/conservation checks.

Both modes repair the pure-advection maximum-principle failure in the bounded
fixture. Mode 5 preserves the retained diffusive accuracy; mode 4 worsens some
upstream errors. The original-spacing leading-tail history remains outside 10%,
so neither is admitted to the wall/application operator. Pointwise smearing into
zero-reference cells is reported as an absolute diagnostic, not a ratio pass
or a new exact-zero acceptance requirement. No final-temperature clipping is used.

### Frozen face and limiter diagnostics

`physics_sim_front_flux_probe.c` observes the actual correction factors and
paired fluxes through a compile-time hook in the diagnostic FCT helper. The
hook does not change the numerical formula. The contained `build_front_arrival.py`
route builds `flux-probe` and `flux-probe-sanitize` alongside the existing tests.
`fire_fluid_front_flux.py --build <build-root> --traces <retained-fct-run-root>
--output <fresh-result-root>` compares exact-reference cell averages and retained
evolved fields at matched physical faces. It also replays a retained 1D case
for exact output parity; it does not run a new coupled benchmark.

Results separate raw/bounded face temperature, raw/bounded gradient, advective
and diffusive flux, total flux before/after update limiting, and limiter activity.
Flux units here are K m/s; conversion to watts requires volumetric heat capacity
and area. Per-patch errors use the L1 norm of the west/east face pair, avoiding a
nearly cancelling net-flux denominator. A limiter factor of one means the
correction is admitted in full; it does not prove the target flux is accurate.

### Steep-tail reconstruction diagnostic (not adopted)

`physics_sim_scalar_exponential.h` provides a compile-time-only positive
face-local exponential for sub-cell decay on positive monotone support. Both
neighboring means are preserved by the fitted profile; value and gradient are
consistent. `PHYSICS_SIM_EXPONENTIAL_TARGET` selects it inside the unchanged
1D FCT budgets. Zeros/extrema and resolved slopes retain the polynomial.
`build_front_arrival.py --output <fresh-root>` builds baseline and exponential
strict/sanitizer tools. `fire_fluid_front_exponential.py --build <root>
--inputs <retained-front-flux-root> --output <fresh-root>` checks the same twelve
128-cell frozen fields, baseline parity and invariants before any trajectories.
The 2026-10-17 candidate passed frozen flux gates but failed transient early-tail
accuracy (37.97%, versus retained 12.06%; gate 10%). It is not an application
default or qualified thermal transport replacement. Retained evidence and the
bounded transient reproducer live in the private front-exponential artifact.

### Sharp-to-zero front repair diagnostic

`physics_sim_scalar_front_target.h` restores the two-cell diffusive gradient
when the retained bounded four-cell derivative is zero despite unequal means.
`PHYSICS_SIM_FRONT_TARGET=1` selects this target within unchanged FCT budgets.
Variants 2/3 are diagnostic ablations adding exponential advection and central
diffusion respectively; neither is selected by the ordinary final builder.
`build_front_arrival.py --output <fresh-root>` also builds `probe-front-zero`,
`flux-front-zero`, `fct-front-zero` and `front-stage-test`, with sanitizer pairs.
`fire_fluid_front_zero.py --build <root> --frozen <front-flux-artifact-root>
--baseline <front-fct-runs> --output <fresh-root>` verifies actual second-cold-cell
transfer, twelve frozen reference fields, baseline parity, and the same slow
128-cell histories at original/half dt plus a pure-advection control.

The repair removes zero-transfer cancellation but is **not transport-qualified**:
early history remains 11.0833%, above 10%; correctly signed under-resolved tail
gradients remain inaccurate. This is a standalone diagnostic, not an application
default. Evidence: private `fire-fluid-front-zero-20261017/RESULT.md`.

### Front-to-smooth positive-profile diagnostic

`physics_sim_scalar_logquadratic.h` fits a positive profile with logarithmic
curvature to monotone positive cell means, with the sharp-to-zero repair as
fallback. `PHYSICS_SIM_LOGQUADRATIC_TARGET=1` selects this target under unchanged
FCT. Variant 2 restricts fit to sub-cell decay and is a rejected ablation.
The fit normalizes adjacent means; outer means constrain symmetric curvature,
not four separate interpolation equalities. It is local to each face.

`build_front_arrival.py --output <fresh-root>` builds `flux-front-smooth`,
`probe-front-smooth`, `fct-front-smooth`, `smooth-stage-test`, `logquadratic-test`
and sanitizer pairs. `fire_fluid_front_smooth.py --build <root> --frozen
<front-flux-root> --baseline <front-fct-runs> --previous <front-zero-root>
--output <fresh-root>` compares the same fields and 128-cell histories.
The main early history passes at 9.5370% (half dt 8.9579%), but weaker early
histories fail/regress. The candidate is not globally transport-qualified or
an application default. Retained startup proof shows a 90.8% first-step heat
release deficit; evidence is in private `fire-fluid-front-smooth-20261017/RESULT.md`.

### Reference continuation attribution

`physics_sim_front_continuation_probe.c` resumes a supplied 128-double cell-mean
field at an aligned physical step, with the existing log-quadratic/FCT operator.
It records actual SSPRK2 face-transfer integrals, advection/diffusion decomposition
and limiter adjustment. CLI: `continuation-front-smooth <dt-denominator>
<first-step> <last-step> <field.bin>`; dt denominator 3200/6400, fixed slow-flow
inputs, endpoint <=1.5 s and observation-aligned times. Input mass/bounds are
validated. `build_front_arrival.py --output <fresh-root>` builds strict/sanitizer
pairs. `fire_fluid_front_continuation.py --build <root> --previous
<front-smooth-artifact-root> --output <fresh-root>` checks native replay,
reference restarts at .025/.1/.5 s, half dt, actual budgets and uniform through-flow.

Reference initialization is a conditional diagnostic, never a production reset
or whole-history acceptance. It shows the dominant tail error is inherited
within the first 25 ms; all subsequent reference-initialized monitored windows
pass 10% while the ordinary startup still fails its weak-tail histories.

### Conservative numerical startup layer (1D diagnostic)

`physics_sim_front_startup_probe.c` retains 16/32 numerical subcells per reported
cell during the initial unresolved thermal layer. `physics_sim_scalar_subcell.h`
provides the positive periodic backward-Euler finite-volume solve. Native input
is only the original sharp field and coefficients; reference is evaluation only.
Actual end-state implicit fluxes and later SSPRK2 face transfers are recorded.
At sqrt(2*alpha*t)>=h, subcells are averaged conservatively once, then the
unchanged log-quadratic/FCT operator continues on 128 cells. Pure advection
bypasses startup. This does not add 3D or application-default behavior.

Builder outputs `startup-front-smooth`, `subcell-test` and sanitizer pairs.
CLI: `startup-front-smooth <10240|20480 steps> <1|16|32 startup-ratio> <u>
<conductivity> <end-step> [1|2 fine-time-refinement]`. Conductivity is 0/.026;
physical duration remains 3.2 s. `fire_fluid_front_startup.py --build <root>
--previous <front-smooth-artifact-root> --output <fresh-root>` qualifies original
startup, first-step space/time behavior, budgets, half dt, exact coarse/pure-
advection controls and sanitizer replay. Selected 32-subcell treatment passes
all monitored reference-qualified 128-cell histories, with first-step error
3.8454% and early main/far/wrap errors 0.7186/0.9446/0.8226%. The 16-subcell
first-step gate fails. Evidence/limits are in private front-startup-20261017.


### Localized startup layer under along-wall prescribed shear

`build_thermal_transport.py --columns --localized-startup --thermal <Thermal>
--shared <shared> --output <fresh-root>` builds a separate y-invariant scalar
qualifier. Run `probe 128 64 5 5120 10` for the single selected 3.2-s history.
It retains 32 x-subcells near the source/wrap (2112 x 64 internal cells) through
the .4-s source plus .08875-s startup interval, conservatively restricts once,
and continues the unchanged 128-cell FCT x operator. Reflecting normal diffusion
uses positive implicit finite volumes. Native first-layer wall support, face
exchange and receiver packets are unchanged; positive mean/deviation lifting
carries actual coarse exchange into retained subcell profiles.

`fire_fluid_localized_columns.py --history <jsonl> --identity <build/identity.json>
--references <retained-reference-dir> --baseline <prior-candidate-jsonl>
--output <fresh-root>` checks source identity, bounds, conservation, given-Q solid
replay, all standard metrics, each local patch and prefix reference convergence.
All standard/reference-qualified local metrics pass 10%; some patches worsen,
and patches 5/6 plus short faint-arrival prefixes remain reference-unqualified.
Those are null gates, not passes. This does not adopt a native 3D/default method.
Detailed evidence/limits: private localized-wall-20261017 RESULT.md.


### Faint-arrival reference qualification

`physics_sim_faint_reference.c` is an independent normal finite-volume/CN
reference with tilted Fourier quadrature in free x and separately summed
physical periodic source images. CLI: `<normal-cells> <modes> <steps> <end-s>
<tilt> <quadrature-period-m> <patch> <image> <observation-stride>`; patch0..7
samples solid strips, -1 downstream gas, -2 near-wall gas, -3 exact x mean
(the last requires one zero-tilt mode and .25-m period). Balanced transform origin
prevents intermediate exponent overflow. Refine normal space, time, mode cutoff,
period and contour independently; cancellation-prone real-axis tails are not an
acceptance oracle. No native solver/receiver calls enter this reference.

`fire_fluid_faint_kernel.py` supplies a positive Robin wall-arrival kernel with
conservative solid-feedback bounds and a separate free-space shear control.
`fire_fluid_faint_audit.py --evidence <root> --previous <localized-wall-root>
--output <fresh-root>` audits retained cases and native histories. See private
faint-reference-20261017 RESULT.md for selected cases and qualification limits.
Short gas/wall histories and local patches now show failures of the unchanged
10% gate; native normal refinement is justified as a next test, not adopted.

`physics_sim_faint_solid_probe.c` links read-only shared core_thermal to reproduce
a separate tiny-signal issue: 50 J/K pair at300K, G2W/K, source1e-16W,
dt1/1600,5120steps. Native back energy is zero versus independent BE1.88418e-17J.
Its successful process return records diagnosis, not a passed accuracy gate.
The shared/Thermal owner must handle that repair separately.


### Bounded normal-space and scalar-time comparison

The columns driver accepts800ticks(.5s),in addition to80/5120. Localized builder
option `--local-micro-factor 2` halves scalar transport/source time during both
retained-subcell and restricted-coarse phases;default1 preserves prior behavior.
Receiver/native ticks and exchange window remain unchanged. Build strict and
sanitizer variants from identical,pinned shared/Thermal inputs to avoid owner
repair drift. `fire_fluid_normal_short.py --base <factor1-build> --half
<factor2-build> --previous <localized-wall-root> --references <columns-refs>
--faint <faint-reference-root> --output <fresh-root>` admits retained64-normal
prefix by exact control,then runs128-normal space/time cases to.5s only.
`--reuse-native` re-audits existing outputs without new gas runs. Rare25/100ms
point values come from separately qualified probes,not the fixed-contour prefix.

Normal refinement improves wall-heat history53.84% to10.45%;half scalar time
leaves10.15%,still failing10%. Gas tail remains30.00%. These controls do not
adopt a new solver/default. The shared tiny-signal repair is now owner-verified
core_thermal0.3.3,but these controlled histories deliberately pin0.3.2;new-core
replay/adoption is a separate step. Evidence/limits:private normal-short-20261017.


### Exact patch6 packet capture and native replay

`build_thermal_transport.py --columns --localized-startup --packet-trace
--core-version <0.3.2|0.3.3> --thermal <pinned-host> --shared <pinned-shared>
--output <fresh-root>` adds standalone `patch6-replay` and sanitizer tools.
Version selection is explicit;0.3.3also checks the owner material-rodSHA.
Default remains0.3.2for historical controls;no app/default adoption is implied.
With trace enabled,probe takes its usual five arguments plus a fresh packet-file
path. `physics_sim_patch6_trace.h` records only successfully admitted patch6
packets,with exact hex doubles and actual microstep allocation;refused staged
batches emit nothing. Default non-trace probe interface and numerics are unchanged.

`physics_sim_patch6_replay.c` reads that exact stream on stdin and uses native
receiver staging for all16strips independently. No gas evolves during replay.
`fire_fluid_patch6_replay.py --old <old-build> --corrected <0.3.3-build>
--previous <localized-wall-root> --faint <qualified-reference-root>
--output <fresh-root>` captures/admit-checks original producer,then verifies
native replay,per-packet accounting,strict/sanitizer equality and independent
80-digit BE distribution. `--reuse-native` re-audits successful retained outputs.
All32local history gates pass10%;six earliest point samples atsubnormal energy
remain failures. Gas/physical-reference gates remain failed;this is not full
corrected gas or feedback qualification. See private patch6-replay-20261017.

### Bounded fine-to-coarse interface comparison

`build_thermal_transport.py --columns --localized-startup --local-micro-factor 2
--interface-trace --local-fine-end <48|64> --thermal <pinned-host> --shared
<pinned-shared> --output <fresh-root>` holds128normal cells and fine scalar
39.0625us/coarse156.25us fixed. End48(default) puts the32:1 jump at3/32;
end64 extends the same fine spacing to1/8. Source, native clocks, exchange,
restriction at.48875s, limiter and10%gate are unchanged. Trace is stderr JSON
from `physics_sim_local_interface_trace.h`: actual BE x-flux integrated over
source-height z band/full y span until restriction; accepted parent profiles
include780/785ticks around restriction. Flux fields stop accumulating after
restriction; they do not claim to represent the later FCT flux.

`fire_fluid_interface.py --base <end48-build> --extended <end64-build>
--previous <normal-short-root> --references <columns-runs> --faint <faint-root>
--output <fresh-root>/runs` runs80-tick strict/sanitizer controls and two800-tick
histories, checks original history exact, ledgers/refusals and unchanged operators.
Its output parent must contain reference-profile qualification artifacts produced
by `physics_sim_interface_reference.c` (512normal,64/128modes,10240steps and
512normal,128modes,20480steps). This wrapper samples the existing reference and
exits at.5s. These local real-axis Fourier profiles FAIL mode qualification and
are explicitly excluded from accuracy claims. The already qualified contour
history and positive wall kernel remain the gates. `--reuse-native` uses retained
identities/logs and validates native hashes without rebuilding/rerunning gas.
`tests/cfd_local_interface_test.c` independently checks a smooth Gaussian face
and one-step cell averages at fixed scalar time; it is not a state replacement.

End64 worsens gas-tail history30.0017% to47.8024%;wall remains10.1493–10.1991%.
The interface undertransmits diffusion (manufactured face62.9777% vs equal-fine
100.0208%), partially masking retained gas-tail error. Neither layout qualifies;
no new default/solver/limiter adoption or feedback. Evidence:private
fire-fluid-interface-20261018/RESULT.md and runs/report.json.

### Uniform-fine source-pulse operator control

`fire_fluid_uniform_pulse.py --build <fresh-build> --output <fresh-results>`
builds strict/sanitizer `physics_sim_uniform_pulse_probe.c` and existing matrix
regressions,then runs diffusion first(u0),prescribed advection second(u0.025m/s).
Each case4096periodic x cells over0.25m,dt1/25600s,source1K/s on[1/32,1/16]
to0.4s,ends0.5s. Observation means on[1/16,3/32]and[3/32,1/8]match physical
supports. Retained fine-stage matrix and add-source-then-BE order are reused;
there is no restriction,z solve,wall/solid feedback,or state replacement. The
later conservative FCT limiter remains unchanged and is outside this control.
`--reuse-native` verifies retained input/artifact hashes and re-audits without
rebuilding or evolving gas. Requires the bundled NumPy Python;no SciPy/mpmath.

`fire_fluid_uniform_reference.py` uses a positive Gaussian rectangle kernel,
age=sqrt transform,64/128/256point quadrature and periodic images for the exact
physical cell averages. Exact4096-cell DFT box weights and60/90-digit Decimal
spectra separately evaluate the spatial semigroup and BE/source-pulse recurrence
at0.4/0.475/0.5s. Continuous Fourier256/512late cross-checks qualify the positive
kernel;early physical signals use the positive kernel,not cancelling spectra.
No native heat cutoff,clipping or tolerance change. Explicit point failures accompany
history gates:diffusion tail history4.3817%passes10%,but14early sampled tail points
fail;advection tail48.7814%fails and all20nonzero tail samples fail. At0.5s,
advection spatial error43.7983%,then BE time adds1.8787%relative to that spatial
solution;native matches exact BE recurrence within4e-12relative at sampled late
points. Dominant retained error is spatial advection,consistent with upwind
numerical diffusion;this does not qualify the coupled simulation or feedback.
Evidence:private fire-fluid-uniform-pulse-20261018/RESULT.md,runs/report.json.

### Conservative higher-order uniform advection candidate

`fire_fluid_uniform_advection.py --build <fresh-build> --output <fresh-results>
--previous <uniform-pulse-20261018-root>` builds a standalone candidate,strict/
sanitizer variants,and targeted/regression tests. It repeats the4096-cell source
pulse,dx1/16384m,dt1/25600s,to0.5s. Five bounded cases:original diffusion,
original advection,SSPRK2donor advection+unchanged BE diffusion,higher-order
advection disabled(u0),then higher-order advection enabled(u0.025m/s).
`--reuse-native` re-audits source/artifact hashes without rebuilding/evolving gas.

`physics_sim_uniform_advection.h` uses the existing fourth-order face
interpolation and paired conservative FCT limiter for advection only(alpha0),
SSPRK2,and the existing BE physical diffusion solve. The diagnostic builder
specializes only original limiter workspace/admission256→4096 and include
routing;original header,limiter equations,budgets and16epsilon headroom remain
unchanged. `capacity-specialization.patch` and source hashes record this exactly.
Original/generated256-cell outputs agree bit-for-bit in direction controls.
Global old-stage extrema comparisons use the existing64epsilon rounding envelope;
negative cells and the existing0.4+1e-10Krise cap are still refused. No clipping,
heat cutoff or state replacement. Three positive-minimum roundoff excursions
(max1.205e-181K)are recorded,not edited;all physical bounds pass.

Original tail-history48.7814%,same-split donor46.4991%,higher-order0.765727%
(pass unchanged10%);main history0.00980578%. Diffusion control is byte-identical
at4.38173%tail. Candidate still fails10early tail samples through0.25s;later
history acceptance is not uniform faint-arrival accuracy.128profile steps,
cold advective/diffusive neighbors,strict/sanitizer equality,17280original
limiter stages and1600matrix regressions pass. Scope is uniform periodic1D,
constant velocity only. No nonuniform/coupled solver adoption or feedback.
Next boundary:nonuniform-interface qualification before returning to the wall
benchmark. Evidence:private fire-fluid-uniform-advection-20261019/RESULT.md and
runs/report.json. The initial over-strict positive-minimum check refusal is
retained separately and resolved by matching the existing validation contract.

### Nonuniform3/32interface qualification

`fire_fluid_nonuniform_advection.py --build <fresh-build> --output <fresh-results>
--previous <uniform-advection-20261019-root> --references <uniform-pulse-20261018-root>`
checks uniform recovery,then2112-cell mixed-grid diffusion/original/donor/candidate
and zero-advection controls. The mesh is fine32subcells/parent forx<48orx>=112,
coarse elsewhere;interface3/32,ratio32. Source/supports,dt1/25600s,0.5sstop and
10%gate are unchanged. `--reuse-native` re-audits retained hashes without a build.

`physics_sim_nonuniform_advection.h` supplies degree3finite-volume face weights
from actual cell widths and the same paired FCT acceptance factors/global budgets/
16epsilon headroom in conserved cell heat. Heat units are normalized to one
finest-cell capacity,so capacities are exactly1or32. This avoids underflow from
multiplying tiny temperatures by small widths. Original limiter/matrix headers
remain byte-identical. The unscaled first attempt produced a negative subnormal
and was rejected;the retained source/refusal shows the repair. No clipping,
floor,extra allowance or physical-bound/gate change.32subnormal cold-transfer
steps,256mixed profile steps,36uniform parity cases and cold neighbor tests pass.

Uniform recovery tail0.765727%;mixed original41.0351%,same-split donor38.8278%,
higher-order5.72366%(pass history10%). Diffusion-only mixed26.7071%fails,
unchanged when advection is disabled. Manufactured limited advective flux is
99.9887%of exact at the jump;unchanged diffusive flux62.9777%. Combined history
acceptance does not clear diffusion or12remaining tail-point failures. Next
bounded boundary is nonuniform diffusion reconstruction before any wall/feedback
integration. Six full strict/sanitizer histories match;original17280limiter and
1600matrix regressions pass. Evidence:private fire-fluid-nonuniform-advection-
20261019/RESULT.md,runs/report.json,underflow-refusal.json,pre-repair-adapter.h.
No coupled/default/shared solver adoption or feedback.

### Local nonuniform diffusion reconstruction repair

`fire_fluid_nonuniform_diffusion.py --build <fresh-build> --output <fresh-results>
--previous <nonuniform-advection-20261019-root> --references
<uniform-pulse-20261018-root>` runs manufactured flux/cold-cell checks first,
then diffusion-only baseline/repair,uniform recovery,and fixed-advection
baseline/repair through0.5s. Same2112-cell mesh,dt1/25600s,source and supports.
`--reuse-native` audits retained hashes without binaries. Advection candidate,
limiter and positive BE matrix source remain byte-identical.

`physics_sim_nonuniform_diffusion.h` keeps that BE solve and adds a paired,
limited gradient-defect correction. Actual-width degree3FVderivative moments
replace the centered gradient only on the six stencils touching width changes
(three around3/32,three around the periodic companion7/32). Uniform stencils
are unchanged. Existing sign/two-central reconstruction guard preserves cold
Fourier transfer;old-stage temperature budgets and16epsilon FCT headroom apply
in normalized cell heat. Same strict nonnegativity,0.4+1e-10Krise cap and64epsilon
rounding comparisons;no clipping,floor,physical-bound or acceptance-gate change.

Manufactured target flux100.618%of exact;actual accepted one-step flux95.6019%
of exact end-time gradient,versus old62.9777%. Diffusion-only tail history
26.7071%→1.51884%;fixed-advection tail5.72366%→0.765039%,both pass10%.
Uniform diffusion recovery is exactly unchanged4.38173%. Early point failures
remain9diffusion/8advected samples;history acceptance is not blanket faint-arrival
accuracy.128new profile steps,32subnormal cold steps,full strict/sanitizer
histories,unchanged advection and limiter/matrix regressions pass. The initial
extra3%face-screen refusal is recorded;all actual acceptance uses the requested
unchanged10%gate. Next boundary:short prescribed-shear128normal benchmark,
no feedback. Evidence:private nonuniform-diffusion-20261019/RESULT.md,runs/report.json.
No coupled/default/shared adoption.

### Bounded prescribed-shear repaired scalar pair

`build_thermal_transport.py --columns --localized-startup --local-micro-factor 2
--solid-trace --core-version 0.3.3 --thermal <pinned-host> --shared <pinned-shared>
--output <fresh-build>` builds the old-pair control. Add `--repaired-scalars` for
nonuniform SSPRK2advection followed by repaired implicit diffusion,one workspace
per operator reused across zrows. Normal/source/wall/coarse/restriction paths
are unchanged. The new profile is diagnostic only. `--solid-trace` captures exact
accepted all-column packets,microallocations and native two-cell energies;it is
exclusive with the older patch6trace. The columns probe then requires a seventh
argv item,the fresh packet path. No accepted-output overwrite.

`fire_fluid_shear_pair.py --baseline <build> --repaired <build> --references
<thermal-columns-20261017/runs> --faint <faint-reference-20261017> --output
<shear-pair-20261020/runs>` runs strict/sanitizer80tick smokes and two800tick
histories. `--reuse-native` rechecks exact source/native identities after build
retirement. Provenance comes from the retained adjacent `provenance.json`.
Existing successful runs are resumed only with matching command and receipt.

The integration's captured16cell/full-row representability refusal is exercised
by `cfd_nonuniform_representable_test.c` reading an exact hex fixture on stdin.
The adapter retains limiter factors;when actual paired temperature updates would
be negative it moves only the common face transfer toward zero to avoid an ulp
withdrawal beyond the donor.64attempt bound;unrepaired cases still refuse.
No temperature clipping,heat floor or bound relaxation. Earlier1Dhistories
remain byte-identical. Strict/sanitizer fixtures and all ordinary scalar/transfer
regressions pass. Results:tail30.001674%→1.220113%pass,main/nearpass;
wall10.149297–10.199060%fail. All256given-Qsolid histories pass,not physical
far-patch short-prefix acceptance. Earliest gas and subnormal solid point
failures stay explicit. Evidence:private shear-pair-20261020/RESULT.md,runs/report.json.

### Wall-normal periodic-x mean diagnostic

`build_thermal_transport.py --columns --normal-mean --core-version 0.3.3
--thermal <pinned-host> --shared <pinned-shared> --output <fresh-build>` compiles
`physics_sim_normal_mean_probe.c` with actual native pair transfer and repaired
receiver. Four transverse x/y cells retain original four-y symmetric exchange;
source is the exact xmean1/8K/s. Source/off and0.48875s scalar transition stay
at matched physical times. A51200Hz diagnostic calendar separates scalar step,
receiver step and exchange cadence. The ordinary scalar pair is not modified.

`fire_fluid_normal_mean.py --build <build> --output <results> --previous
<shear-pair-20261020-root>` runs eight full strict/sanitizer0.5smean histories,
packet/solid Decimal checks,and three independent same-normal CNcontrols.
`--reuse-native` audits retained source/output hashes after build retirement.
CLI probe args:normal cells128/256/512,scalar calendar ticks1/2,exchange ticks
40/80/160,receiver ticks-per-second1600/3200/6400,end calendar ticks2560/25600,
and fresh packet path. Only128/256and full0.5sare qualified here.

128half-scalar still fails10.000594–10.050290%wall history;256normal passes.
Independent spatial error13.55–13.60%→3.30660–3.35328%;preferred256half-
scalar/quarter-exchange mean candidate2.554766–2.601098%. Refined cadence
removes cancellation rather than selecting the smallest raw error. Early points
still fail independently of time convergence. No full shear/default adoption
or feedback;source/receiver/normal numerical operators unchanged. Evidence:
private normal-mean-20261020/RESULT.md,runs/report.json. Audit handoff preserves
predecessor builder bytes and all command/native/source identities.

### Selected profile in the prescribed-shear diagnostic

Add `--shear-normal256` to the columns/localized/repaired-scalars/solid-trace
builder flags to select native 128×4×256 and the physical 6400 Hz clock. The
profile has fine scalar step 1/51200 s, coarse step 1/12800 s and five-tick
exchange/receiver windows. Source-off tick 2560, restriction tick 3128 and
observation stride 160 preserve 0.4/0.48875/0.025 s. The legacy profile retains
its old clocks and numerical observations. Only profile-owned admission/storage
and scheduling were changed; no global grid cap, scalar formula or wall law.
`physics_sim_shear_clock.h` is shared by this diagnostic driver and its scalar
module. The localized branch does not use the generic open-engine scalar default.

`fire_fluid_shear_integrated.py` runs the declared legacy smoke, selected strict/
sanitizer smoke and one selected full run. Selected probe args are
`128 256 5 3200 10 <fresh-packet-path>`, with a short-only profile admission.
The new exact hex packet header v2 records the 6400 Hz clock. A fourth refusal
control rejects after real gas/solid candidate work, then checks unchanged
accepted state and recovery; eight controls total. `fire_fluid_shear_local_reference.py`
checks short modal space/time/mode refinement, with exact prior-prefix control.
`fire_fluid_shear_integrated_audit.py` audits retained native/source identities,
physical histories, all-column Decimal given-Q response and qualified local
histories without a binary. `physical-observations.jsonl` adds explicit times
and interval-average rates without mutating the original receipts.

Selected gas, total heat/rate and three local physical wall/solid histories pass
10%; five local references and earliest faint points remain unresolved/failing.
No automatic ladder follows. Evidence and a user-review-only heated-solid
proposal: private shear-integrated-20261020/RESULT.md,report.json,
HEATED_SOLID_PROPOSAL.md. No live thermal return-channel or default adoption.

- `physics_sim_warm_solid_probe.c` — separate approved5s warm/off/reversal
  diagnostic,256normal cells,1/51200s scalar,nominal5/6400s exchange split at
  0.01s observation boundaries,1/6400s receiver. Actual wall kernel and frozen
  receiver, uniform transverse reduction;21K fixture bound, old0.4Kcap unchanged.
  Fixed one-step receiver packets preserve event alignment and exact heat totals.
  `physics_sim_warm_solid_reference.c` independently integrates CN normal FV and
  the authored two-cell solid; `fire_fluid_warm_solid.py` audits retained output
  and generates standalone plots. Warm/history/control tests pass; seven reversal
  startup gas points still fail10%. No live integration or all-point qualification.

- Warm-solid optional bounded reversal CLI: `probe 2 <fresh-packets>
  <scalar-factor:1|2|4> <exchange-receiver-steps:1|2|5>` stops at20ms, normal256,
  receiver1/6400s; default three-argument5s run unchanged. The separate
  `scripts/fire_fluid_reversal_timing.py` runs/audits the declared3x3matrix.
  Exchange cadence improves startup error more than scalar time; finest observed
  point error16.19%still fails10%, with same-grid spatial reference14.25%.

- Selected `wall-refined` warm-solid solver: `probe <case:0|1|2> <fresh-packets>
  wall-refined`. It actually solves 512 normal cells with conservative CN face
  fluxes, compensated increments and centered one-receiver-step wall exchange.
  `physics_sim_normal_conservative.h` implements the bounded uniform-normal
  theta step; `tests/cfd_normal_conservative_test.c` covers constant/source
  invariance, BE parity, analytic CN decay, cold/subnormal transfer and refusal.
  Startup error 3.67%; declared five-second warm/off/reversal gates pass. Old
  faint and extra near-zero signed-change point failures remain separate.
  `fire_fluid_wall_refined.py` audits exact retained qualification data.

- `physics_sim_thermal_step.c/.h` — single numerical step used by the warm-solid
  qualifier and joint consumer; `physics_sim_joint_thermal.c/.h`,
  `physics_sim_joint_checkpoint.c` and `physics_sim_joint_thermal_cli.c` own
  accepted state, fixed-profile admission, durable joint checkpoint and local
  CLI. See [runtime contract](../../docs/fire_fluid_joint_runtime.md). Build with
  `build_thermal_transport.py --joint-thermal --warm-solid --columns
  --core-version 0.3.3` and the frozen Thermal inputs. No live channel/default
  adoption. `tests/cfd_joint_thermal_test.c` covers atomic refusal/recovery.

- Online receiver option: `build_thermal_transport.py --live-receiver
  --joint-thermal --warm-solid --columns --core-version 0.3.3` builds the private
  `receiver-worker`/`receiver-worker-sanitize`. Append its absolute path to the
  joint CLI to stage each actual wall packet through a separate Thermal process.
  `physics_sim_receiver_fixture.h`, `physics_sim_receiver_wire.h` and
  `physics_sim_receiver_client.c/.h` own fixed fixture, bounded field codec and
  process lifetime. `tests/cfd_receiver_process_test.c` covers refusal/death/lost
  response and retry without independent solid commit. See the joint runtime
  contract; this does not connect to an installed Thermal workbench/session.

- The live receiver route also supports selected `--shear-normal256` with its
  existing repaired scalar pair and solid trace. Append a receiver path to
  `probe 128 256 5 3200 10 NEW_PACKETS`; append `restart` to test child replacement
  around source-off/restriction. `physics_sim_shear_receiver_fixture.h` preserves
  all128physical supports and exact clocks. `cfd_shear_receiver_process_test.c`
  replays all81920column windows against actual retained energies, including
  faint/subnormal transfer. Worker request idle wait is60s; body/reply wait5s.
  `--durable-shear` separately enables whole spatial-host range checkpoints. See
  [headless runtime contract](../../docs/fire_fluid_joint_runtime.md).

- `physics_sim_shear_checkpoint.c/.h` stores the fixed spatial benchmark's native
  gas, fine/coarse localized field, all receiver states, ledgers, clocks and
  pending range identity. `physics_sim_localized_state.h` is the accepted-state
  persistence seam. The manifest-verifying `scripts/run_shear_durable.py` accepts
  explicit range ends; same-target recovery replays from the accepted checkpoint,
  while an already committed end advances zero steps. The receiver remains a
  stateless staging process. `tests/cfd_shear_checkpoint_test.c` checks atomic
  publication, identity, corruption and non-mutating refusal. Trace exports are
  speculative until the range receipt commits; see the runtime contract.

- `physics_sim_tail_diffusion.h` / `physics_sim_tail_fit.h` — opt-in conservative
  early-tail diffusion candidate, enabled by `build_thermal_transport.py
  --tail-diffusion` on the selected 256-normal profile. Native five-tick and fine
  scalar clocks stay fixed; the profile is limited to 0.05/0.1 s and excludes
  the old durable checkpoint build. See [method and bounded physical verdict](../../docs/fire_fluid_early_tail.md).
  `physics_sim_early_tail_probe.c` isolates source/time/space error and
  `tests/cfd_tail_diffusion_test.c` covers moments, mass, cell bounds, cold and
  subnormal transfer. `fire_fluid_early_tail.py` audits coupled exports; the
  earlier 0.025 s arrival remains unqualified.

## Experimental consistent-stage gas diffusion

`physics_sim_tail_time.h`uses the retained reconstructed, paired-FCT-bounded
flux throughout SSPRK(10,4), without the old two-point implicit prediction.
The optional build flag`--tail-time`requires`--tail-diffusion`. It changes
internal stages while retaining physical scalar/native/exchange clocks.
The original-clock1-D0.025s physical gates still fail, so its compiled native
hooks are not a coupled qualification or default/feedback adoption. Probe
methods5/6/7 select10/20/40 internal evaluations with the same outer source
schedule. See[method and limits](../../docs/fire_fluid_early_tail.md).

## Latest source-interval repair: experimental 1-D only

Probe method 8 integrates conduction of newly generated source heat during each
original scalar tick, rather than endpoint half-kicks. Existing-field SSPRK(10,4),
reconstruction, paired limiter, bounds, physical supports/clocks and 10% gate stay
fixed. No reference-state replacement or heater-footprint expansion occurs.

At 0.025 s normal wall-cell error improves -70.3916% to -7.6798%; uniform and
unequal horizontal errors improve to -2.2105%/-2.3479%. The normal band improves
-30.9033% to **-10.9626% and still fails**. All 0.1 s gas-only controls pass.
The old first cold normal-cell deficit starts on source tick one (-97.8408%);
source integration removes that initial deficit, while later normal transport
still develops error. This does not qualify an entire early history or wall/solid
response. Thin-front reconstruction controls were rejected.

Fresh strict/ASan-UBSan source, bounds, conservation and six affected operator
regressions pass; the three 0.025 s exports match exactly across builds. Method 8
is 1-D only; no new native receiver run or feedback. Source edges require four
locally uniform cells per side and a <=1e-12 omitted-tail guard. Next localize the
remaining normal-band transport deficit on the same source-aware field.
Evidence: `_private_workspace_artifacts/fire-fluid-startup-deficit-20261021`.

## Latest normal-band flux investigation and fit quadrature repair

The same source-aware 1-D grid/clocks isolate the normal-band deficit to gas
heat distribution. Requested/admitted boundary transfer is 1.0 with no limiting;
its heat ledger closes to 1.34e-15 relative. On analytic cell means the late band
face gradient is accurate, but the heater-edge fit is 26–42% low during early
startup. A smooth reconstruction across the discontinuous source remains the
next bounded repair target; no new wall/solid error is established.

A separate demonstrated arithmetic defect is fixed in `physics_sim_tail_fit.h`:
bounded composite Gauss integration resolves steep exponential variation. The
existing limiter, bounds, source interval, time operator, grid and 10% gate stay
fixed. At 0.025 s the normal band moves -10.9626% to **-10.9465%, still failing**;
normal first cell -7.6085%, uniform/unequal horizontal -2.2105%/-2.2779% pass.
The 0.1 s gas-only controls pass. No entire early-history qualification.

Fresh strict/ASan-UBSan builds, 436 independent moment/fit checks, seven affected
source/operator tests and exact five-history parity pass. No new coupled run,
default adoption, shared Thermal change or feedback. Evidence:
`_private_workspace_artifacts/fire-fluid-normal-band-20261021/RESULT.md`.

## Latest heater-edge repair: selected 1-D gates pass

Explicit probe method 9 fits a finite-duration heated-step gradient at declared
source jumps, including post-heating spreading within diffusion stages. It uses
local means, source power and diffusivity, then passes the requested flux through
the unchanged conservative limiter. Source energy, grid, clocks, actual bounds,
previous source/time/quadrature repairs and 10% gate remain fixed.

52 independent analytic cases pass; the same0.025 s history gives normal first
cell -5.6004%, normal band -9.7642%, uniform/unequal horizontal +1.7623%/+1.6921%.
All selected0.1 s controls pass. The normal-band margin is only0.2358 percentage
points. A continuously heated profile that ignored stage spreading was rejected.

Earliest startup improves: first cold normal-cell error after four ticks falls
+31.25% to +0.67%. Later profiles often leave the admissible model family and use
the existing fallback; earlier faint remote tails and the whole history remain
unqualified. No wall/solid result follows from these gas-only diffusion probes.

Fresh strict/ASan-UBSan nine test programs,52 analytic and five history parity
checks pass; all five old method8 controls remain exact. Method9 is1-D only;
no native prescribed-flow/coupled integration or feedback. Next test its
applicability under prescribed advection before wall/solid attribution.
Evidence:`_private_workspace_artifacts/fire-fluid-heater-edge-20261021/RESULT.md`.

## Latest prescribed-flow reduction: advection face repair passes selected gates

The actual unequal2112-cell periodic source-pulse fixture, prescribed u=.025 m/s,
fixed scalar dt1/51200 and original supports exposes a new early-tail failure:
heater-edge method9 alone gives-99.9902% at0.025 s despite passing zero-flow
controls. On analytic cell means, the fine-to-coarse polynomial reconstructs a
negative temperature rise at x=3/32 (face error-406.47%).

Opt-in advection method3 uses the existing positive finite-volume profile fit
for steep/unequal faces, retaining the paired conservative limiter and actual
bounds. Flow-probe method10 combines it with heater-edge method9. Tail errors
become+7.7798% at0.025 s and-1.5395% at0.1 s, passing the fixed10% gate; transport
band and heater-edge cell errors stay below0.04%. Independent reference gate0.5%
is unchanged. No source/time/grid change or state replacement.

Ten test programs pass strict/ASan-UBSan, including196 positive-tail checks;
selected moving/zero-flow histories match across builds and legacy controls
remain exact. This is1-D translation only. Full spatially varying shear,
wall/solid response, earlier faint-history and feedback remain unqualified.
Next use a bounded gas-only shear prefix before a longer trajectory.
Evidence:`_private_workspace_artifacts/fire-fluid-prescribed-gas-20261021/RESULT.md`.

- `physics_sim_shear_gas_probe.c` — actual2112x256 y-invariant spatial-shear
  gas-only profile through160 native ticks/.025 s; explicit wall bypass, fixed
  clocks/limiter/bounds. `physics_sim_source_patch.h` prepares the one joint-age
  continuous rectangular source increment; `physics_sim_shear_gas_lines.h`
  optionally batches independent lines with separate workspaces. No default
  scalar/coupled adoption. `physics_sim_shear_gas_reference.c` is an independent
  positive shear kernel through.025 s; optional segment0..7 divides only the
  original normal observation support. `physics_sim_shear_gas_field_probe.c`
  reads the final frozen field without advancement. Four endpoint gates pass;
  startup history fails. Reproduction/evidence:
  `_private_workspace_artifacts/fire-fluid-shear-gas-20261021/REPRODUCE.md`.

- `physics_sim_normal_startup_probe.c` — fixed256-cell normal source history;
  profiles0 legacy,1 net-transfer precision,2 complete source support,3 both.
  Internal diffusion subcycles are diagnostic; source/scalar clock remains
  1/51200s. Complete source API supports0..1K/s and requires uniform kernel
  support through natural underflow. No source/observation support is changed.
  The new mode3 in `physics_sim_tail_time.h` uses mode2 reconstruction with
  bounded net-transfer application; it is not a higher spatial order.
  Mechanism tests pass but selected physical histories fail10%; no adoption.
  Evidence: `_private_workspace_artifacts/fire-fluid-startup-flux-repair-20261021`.
