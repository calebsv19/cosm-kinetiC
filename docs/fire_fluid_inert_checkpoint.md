# Optional inert object in live Fire–Fluid checkpoints

This source-development option adds one authored isothermal object face to the
existing headless native Fluid participant. Fire consumes accepted-prior gas
wind as before. The object shares the gas clock and checkpoint, with joint
publication through the existing Fire–Fluid Framework transaction. It does not
add a third independently scheduled participant or a separate ThermalSim worker.

## Closed configuration and native state

Add `inert_object` to an ordinary open-atmosphere fluid configuration containing
an interior stationary `solid_box`:

```json
{
  "schema": "physics_sim_inert_object/v1",
  "axis": 0,
  "side": -1,
  "initial_temperature_k": 300.0,
  "capacity_j_k": 10.0,
  "h_w_m2_k": 2.0,
  "support_depth_m": 0.125
}
```

The face footprint is the selected box face. Depth is a positive grid-aligned
extrusion into available gas. Axis is0–2; side is−1/+1. Temperature starts at or
above the gas reference; C is positive and h nonnegative. The underlying
solid-cell operator remains insulated, with no heat/smoke stored in solid gas
cells. Explicit conservative object exchange is a separate signed gas debit.
The authored fixed-depth nonlocal Robin law is not a resolved wall-gradient or
measured material convection model. No ignition/radiation is introduced.

State and fields both contain `inert_object`, schema
`physics_sim_inert_object_state/v1`, with `temperature_k`,
`temperature_correction_k`, `gas_to_object_j`, `steps` and `time_s`. The correction
retains weak increments below the300K temperature rounding scale. Config/worker
hashes, state digest, matching object/gas clock, compensated representation and
object energy/receipt consistency are checked on continuation. Unknown/missing
fields or changed configuration refuse. Omitting the option omits new fields.

## Execution, energy and publication

Each native step computes gas transport then applies the existing conservative
exchange at the same dt. Failure produces no accepted native result. Python
independently checks gas stored−initial−source−inflow+outflow+signed object heat,
including the interval difference of the cumulative receipt. A negative receipt
means object cooling supplies gas heat. The receipt is not counted as new Fire
input. Object energy change C×(temperature−initial+correction) must equal signed
transfer within1e−9J. Existing relative gas/flux gates retain their tolerance;
exchange magnitude contributes to their accounting scale, including cooling.

The complete native substate is included in the existing state/result/joint
digests and lossless journal. Fire, gas and object publish in one SQLite
transaction. Prepublication failure leaves the accepted history and operation
receipt absent; retry consumes the same accepted parent. Duplicate/stale
operation rules are unchanged. Power-loss and concurrent-writer qualification
are outside this controlled recovery proof.

## Recipes and verification

`coupling.inert_cases.recipe(workers, live=True)` supplies the bounded16³/Fire17
warm source case. It uses a low box, upstream floor source, .125m support,
0.005s gas steps,0.02s Fire and0.1s exchange over1.2s. Its source footprint and
ignition region are explicit; authored SI calibration is unchanged. The
controlled-heater recipe reuses the prior source-to-object geometry and40J
pulse, without native Fire.

Build `open-atmosphere-worker` or `open-inert-sanitize` in a fresh owned
BUILD_DIR; preserve retained workers. Run `tests/test_inert_checkpoint.py` with
existing FIRE_FLUID_* worker/root variables, a fresh `INERT_TEST_ROOT`, and
`INERT_PREVIOUS_TRANSFER_J` from the retained16³controlled heater. New tests use
retained task-owned attempt roots so failures can be inspected before cleanup.

## 2026-10-29 — Durable live Fire–Fluid object state completed

Optional native Fluid `inert_object` now binds face/support/initial temperature,
capacity and transfer coefficient. Its checkpoint carries temperature, the
compensated low part, cumulative signed gas→object heat and exact gas step/time.
The original conservative exchange runs each native gas step; object/gas share
one Fluid checkpoint and the existing joint Fire–Fluid SQLite transaction.
Core Sim still schedules two participants. The separate gas debit is included
in lifetime/window energy checks; it is never relabeled as source input.

All13joint checkpoint digests match uninterrupted versus restarted live Fire.
An injected prepublication exception leaves accepted head and operation table
unchanged; retry is exact, duplicate operation returns its receipt and a stale
new operation refuses. A fresh OS process advances a copied accepted6-window
prefix to the identical final12-window digest. Original journals are untouched.
This qualifies controlled process/software recovery, not power loss/concurrency.

The16³gas/Fire17×17 warm native source transfers0.000417804491J into the object, giving
4.17804491e-05K rise after1.2s. Actual gas source input is0.887872957J; gas stores0.886990486J and
outflow is0.000464665924J. Exchange-off object remains exactly unchanged. The warm source
retains authored SI calibration; no gain increase was used. Independent full
journal audits verify source consumption, clocks, held-return causality and
separate gas/object/outflow budgets. The small signal is observable numerically,
not evidence of visible material heating or physically calibrated hot fire.

Four new native checkpoint tests,39existing Python checks,320-step bitwise
legacy native parity, Address/Undefined heating/restore/cooling probes, fresh
native/app compile and broad app test-fast pass. Prior cold-start controlled
heater transfer also matches exactly. New role-specific native codec and Python
admission modules reuse existing exchange/core_sim; no shared/Growth source
or minimum-version change. Original omission/default paths remain unchanged.

Evidence: `_private_workspace_artifacts/fire-fluid-inert-checkpoint-20261029/RESULT.md`.
API: Main Edit `docs/fire_fluid_inert_checkpoint.md`. Source is uncommitted Main
Edit; no commit, VERSION, release, install, Desktop or remote effect. Minor
release recommended, requiring approval before version/release actions. Next:
longer source-off/recovery ventilation with this durable object path, physically
justified source/heat budget and targeted grid/time sensitivity before renders.


Historical journal bindings are immutable. Current control changes do not rebind
older scientific runs; preserve their worker/control bytes for inspection.
This is local source behavior, not an installed app/worker or release claim.
