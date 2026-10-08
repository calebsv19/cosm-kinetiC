# core_sim

Shared simulation control-plane foundation.

## Whole-framework qualification (T6, v0.8.1)

The additive model-time, schedule, exchange and joint-progress foundation is
qualified in shared development source. Fresh strict C11 and ASan/UBSan runs
pass all five C suites and the optional reference host's five groups / 53
process cases per build. Each public header compiles independently and together;
legacy-loop behavior and the optional trace adapter retain regression parity.
T6 changes docs/qualification only, so the source version remains 0.8.1.

This is source qualification. Real solver adapters, numerical field validation,
app adoption, installed packages and remote workers require their own proof.
The fixed-step policy requires every participant period to divide the exchange
window; observations may instead use exact brackets. Core owns metadata and
proposed joint progress; hosts own native state, physical budgets, referenced
content verification and durable publication. The POSIX reference host below
uses toy quantities and qualifies controlled process exits, not power loss or
concurrent publication.


## Standalone reference host (v0.8.1, T5)

The optional POSIX example under `examples/reference_*` combines T1–T4 without
changing their APIs. It runs two integer toy participants for 40 model seconds:
a source at 1/50 s, receiver at 1/200 s, and exchange at 1/5 s. T1 derives a
600-tick/s timebase including a fixed 60-Hz observation representation. Native
quantities are exact toy quanta; this is not a combustion or fluid model.

The source emits one quantum per model tick at baseline. The receiver returns
zero until it has received source, then one. That accepted return value is held
for the next window. Nonzero gain doubles the source rate after the first
window. T3 uniform overlap fractions divide each integrated amount among 40
receiver steps. Each host window checks actual counter/budget conservation
before constructing T4 consumption declarations and a proposed joint record.

| Mode | Meaning | Final emitted / received |
|---|---|---|
| 0 | Return connection disabled | 24,000 / 24,000 |
| 1 | Return connection enabled, zero gain | 24,000 / 24,000 |
| 2 | Return connection enabled, unit gain | 47,880 / 47,880 |

All modes execute 200 windows, 2,000 source steps and 8,000 receiver steps.
Independent Python oracles use closed-form solutions, not the C stepping loop.
5/25/60 FPS changes only observations: numerical trajectory identities remain
identical. Observation brackets interpolate actual retained fine-step samples
with rational weights, including fractional brackets at 60 FPS. No observation
feeds numerical advancement. Output separates model ticks from elapsed monotonic
wall nanoseconds.

```sh
make -C shared/core/core_sim OBJ_DIR=/tmp/core-sim-reference-build reference test-reference
/tmp/core-sim-reference-build/core_sim_reference --checkpoint /tmp/core-sim-demo.checkpoint --mode 2 --fps 25 --stop-after 73
/tmp/core-sim-reference-build/core_sim_reference --checkpoint /tmp/core-sim-demo.checkpoint --mode 2 --fps 60 --resume
```

Choose a fresh checkpoint filename: initial publication refuses existing files.
`--resume` validates and restores the same plan/native checkpoint; it does not
replay accepted solver steps. The example-owned canonical text format stores
native state, prior state, profile and numerical trajectory identity. Its fixed
versioned recipe reconstructs all active T4 metadata without raw C-struct dumps
or replaying numerical advancement. FNV detects accidental corruption; it is
not authentication. This is a single-writer example, not a universal format.

Publication writes a sibling temporary file, fsyncs, atomically links the initial
file or renames updates, then fsyncs the directory. Only afterward does the host
adopt the proposed state. Controlled failures after source/receiver computation
leave staged state unaccepted. An injected exit after publication proves that a
new process can reread the same checkpoint and continue without duplication.
This tests controlled process exits; power-loss and concurrent-writer behavior
are not qualified. Bad versions/profiles, damaged files and invalid native
budgets reject. Failed fresh runs preserve existing checkpoints.

`reference` and `test-reference` are optional POSIX/Python-stdlib targets; the
base library remains C-standard-library-only. `make test` retains the five core
C suites. T5 adds no library function or semantic change; 0.8.1 is a PATCH for
example/test qualification. T6 whole-framework qualification is complete; real adapters remain a separate adoption stage.

## Joint progress (v0.8.0, T4)

The optional `core_sim_progress.h` API validates complete exchange-window
candidates and produces a proposed accepted-state transition. It reuses the
T1 time, T2 schedule and T3 channel rules; no native solver is called.

A frozen joint plan declares every participant's explicit phase order, required
channels, and a host-verified immutable plan reference. Checkpoints are listed
in schedule participant order and transfers in channel order. Every candidate
binds an immutable identity, expected predecessor, next window, a complete set
of endpoint checkpoints, and consumption declarations for every required channel.
Nonzero validation references bind host-owned native/content/budget evidence;
the core checks references structurally and does not authenticate their contents.

Each transfer binds its records to its producer's prior or staged checkpoint
and to the staged consumer checkpoint that declares consumption. A staged
producer is available only if its phase precedes the consumer. This admits
Fluid-at-window-start → Fire → Fluid without recursive solver calls. Integrated
amounts must cover the entire window under v1; point/discrete samples use the
T3 sampling policy. A linear bracket can use prior/staged producer endpoints
when the explicit phase order makes both available. Physical meaning and
consumption remain adapter-verified.

The bounded v1 plan admits up to 256 participants and 64 required channels,
one whole-window advancement per participant and one transfer per channel.
Adapters aggregate finer source records before this boundary. Zero channels
are allowed for independent participants; complete checkpoint sets are still
required. Partial windows, missing evidence references, wrong checkpoint times,
foreign identities and unsupported phase dependencies reject.

`CoreSimJointProgress` retains the last accepted candidate plus its predecessor
checkpoint set. Restart validation checks that metadata against the supplied
frozen plan. Exact last-candidate retries return `REPLAY`; a changed candidate
under the same identity, or a different candidate claiming the same accepted
window/predecessor, conflicts. Older requests reject as stale or wrong-window.
This is a compact last-window ledger, not an unbounded history lookup. Strict
consecutive windows prevent accepting earlier transfer keys again; the host
must also verify immutable identity/content and retain any full audit history.

```c
#include "core_sim_progress.h"
CoreSimJointProgress proposed;
CoreSimJointStatus status = core_sim_joint_accept(
    &joint_plan, &current, &candidate, &proposed);
/* On OK: host atomically publishes proposed and only then exposes it as current.
 * On REPLAY: preserve current; no transfer is accepted twice.
 * On other status: preserve current and restore/discard staged native outputs
 * according to the host checkpoint policy. */
```

Every non-OK status preserves outputs, including `REPLAY`. `out == current` is
supported for the in-memory transition, but durable hosts should prepare into a
separate object, publish, then expose that object. Other output/input overlap is
unsupported. If publication is uncertain, reread the same identity before retry.
Core Sim performs no IO, locking, native rollback, automatic retry, allocation,
clock reads or publication. It cannot undo a failed solver callback. Value structs
are not a persistence format. T5's standalone reference harness and real app
adoption remain separate work.

## Typed exchange metadata (v0.7.0, T3)

The optional `core_sim_exchange.h` API reuses exact time and independent schedules
for structural admission of channels and records. Channel descriptors bind a
plan, producer/consumer participants, quantity, unit, spatial support and
adapter/profile IDs. Those IDs are opaque host identities; adapters verify their
physical meaning and referenced contents. Semantic version 1 is supported;
unknown versions or required features reject. These C values are not a wire
format, and channels/plans must remain frozen under their identities.

| Temporal kind | Allowed policy | Result |
|---|---|---|
| Interval-integrated amount | Explicit uniform rate | Exact overlap duration and fractions of source/target intervals |
| Point observation | Exact, bounded hold, linear bracket | Selected sequences, rational upper weight and held-sample age |
| Discrete state/event snapshot | Exact or bounded hold | No numeric blending; sequence identifies channel-local order |

An integrated quantity such as emitted mass or heat is an amount over a
nonempty half-open interval. It cannot be admitted as a point observation.
Uniform reconstruction must be explicitly declared. The overlap helper returns
zero for touching/disjoint intervals. Adapters conserve an amount by covering
its support exactly once with disjoint target intervals; the helper tracks no
allocation ledger, physical array or budget tolerance.

Point and discrete records bind samples to their checkpoint time. Integrated
support belongs to its source exchange window and ends at its checkpoint time.
The adapter validates native checkpoint cadence and dense-output meaning.
Each record binds its plan/channel/window/sequence key to checkpoint and payload
references. Pairwise comparison distinguishes replay, conflict and distinct
keys without mutating an acceptance ledger. Referenced content must be immutable
and verified by the host; changing content requires a new reference identity.

Sampling takes an explicit producer-availability frontier. Every supplied record
must be available through that time. Exact sampling requires a matching sample;
hold uses a past sample with an explicit positive maximum age; linear requires
two ordered available records bracketing the request, or one exact record.
Missing brackets, stale holds and future-only input reject without fallback or
extrapolation. A linear upper sample can be later than the request only if it
is already available. Held inputs may be used later than the frontier within
the declared age limit. Requests belong to their record's exchange window;
observation endpoints include the window end. Older point samples can be
explicitly rebound to a later window for hold admission.

```c
#include "core_sim_exchange.h"
/* Given a validated schedule, declared integrated channel, host-verified
 * immutable source record and available frontier: */
CoreSimExchangeOverlap weights;
CoreSimExchangeStatus status = core_sim_exchange_overlap(
    &plan, &channel, &source, available_through, receiver_interval, &weights);
/* Check status. Under the declared uniform-rate profile, the adapter allocates
 * source_amount * weights.source_fraction without changing the physical unit. */
```

The API reads borrowed immutable inputs for the call and retains no pointers.
Outputs must not overlap inputs; failures preserve outputs. It performs no
native field interpolation, content verification, unit conversion, event queue
execution, solver advancement or accepted-ledger mutation. These are structural
temporal rules; host/adapter verification is still required. T4 joint acceptance
and app adoption remain separate.

## Independent schedules (v0.6.0, T2)

The optional `core_sim_schedule.h` API builds on exact model time. Each
participant declares its own positive fixed step. A plan declares a nonempty
horizon and a positive exchange period; every participant step must divide that
period exactly, and the horizon must contain whole exchange windows. All values
must share the same domain and timebase. Nonzero plan and participant identities
are caller assigned; participant identities must be unique.

Index-based queries enumerate exchange windows, individual participant steps,
and each window's first global step index and step count without allocating an
event array. Observation queries include both horizon endpoints and return the
neighboring state times, completed-step indices and a reduced dimensionless
rational weight. Exact state observations return equal endpoints and weight
0/1. Shifted horizon origins are supported.

```c
#include "core_sim_schedule.h"

CoreSimTimebase base = {1, 200};
CoreSimParticipantSchedule participants[] = {
    {1, {base, 4}}, /* Fire: 1/50 s */
    {2, {base, 1}}  /* Fluid: 1/200 s */
};
CoreSimTimeInterval horizon = {{base, 0}, {base, 8000}};
CoreSimSchedule plan;
CoreSimScheduleStatus status = core_sim_schedule_init(
    1, horizon, (CoreSimDuration){base, 40}, participants, 2, &plan);
/* Check status before querying plan; keep participants alive and immutable. */
(void)status;
```

This 40-second plan has 200 exchange windows, 2,000 Fire steps and 8,000 Fluid
steps regardless of observation FPS. The API accepts 1–256 participants to bound
validation work. Queries revalidate borrowed participant storage, but cannot
detect a valid edit between calls; assign a new plan identity when changing it.
All failures preserve outputs. Outputs must not overlap borrowed plan storage.

Brackets describe planned sample locations, not snapshot availability or
permission to consume a future state. T2 performs no field interpolation,
solver advancement, clock reads, allocation or exchange payload admission.
Standalone tests cover counts, contiguous enumeration, reversed fast/slow
roles, shifted horizons, FPS invariance, rational bracket oracles, domain and
alignment rejection, output preservation and UINT64 limits.

## Exact model time (v0.5.0, T1)

The optional `core_sim_time.h` API adds nonnegative, exact model-time values.
It is independent of `core_sim_loop_advance`, `core_time`, wall-clock budgets,
render FPS and solver state. Existing loop structs and algorithms are unchanged.

- `CoreSimTimebase`: nonzero caller-assigned run/branch domain and integer
  ticks per second. The complete pair must agree for arithmetic/comparison.
  Callers must not reuse a domain for a different run origin.
- `CoreSimTimePoint` and `CoreSimDuration`: distinct C structs with a copied
  timebase and uint64 ticks. A zero elapsed duration is valid arithmetic;
  solver cadences are admitted as positive by the optional schedule API.
- `CoreSimTimeRatio`: nonnegative rational seconds, normalized on conversion.
  Positive rational periods derive the smallest integer timebase with checked
  denominator LCM, caller resolution limit and period-tick overflow checks.
- Checked add, ordered difference, comparison, exact duration scaling and
  nonempty half-open interval construction. Errors preserve output objects.
  Unrepresentable fractional ticks, overflow, reversed intervals and mismatched
  domains return distinct statuses; arithmetic never rounds or saturates.

For Fire 1/50 s, Fluid 1/200 s and exchange 1/5 s, the derived rate is
200 ticks/s: durations are 4, 1 and 40 ticks. Including a 1/60 s period gives
600 ticks/s. Numerator/denominator inputs avoid binary-float rationalization.
Conversions back to seconds return reduced rational values, not floating-point
approximations. Float admission/conversion at solver boundaries is a later
adapter responsibility. No schedule, interpolation, exchange engine, clock
provider, allocation or persistence format is introduced by T1.

```c
#include "core_sim_time.h"

CoreSimTimeRatio periods[] = {{1, 50}, {1, 200}, {1, 5}};
CoreSimTimebase base;
CoreSimDuration fire_step;
CoreSimTimePoint origin, after_one_step;
/* Check every returned status before using its output. */
if (core_sim_timebase_from_periods(1, periods, 3, 1000000, &base) == CORE_SIM_TIME_OK &&
    core_sim_duration_from_seconds(base, periods[0], &fire_step) == CORE_SIM_TIME_OK &&
    core_sim_time_point_make(base, 0, &origin) == CORE_SIM_TIME_OK) {
    CoreSimTimeStatus status = core_sim_time_point_add(origin, fire_step, &after_one_step);
    (void)status;
}
```

The standalone tests cover known physical durations, exhaustive small rational
oracles, UINT64 limits, reduction before scaling, invalid inputs, identity
rejection and error-output preservation. Use a fresh output directory to avoid
touching retained build artifacts:

```sh
make -C shared/core/core_sim OBJ_DIR=/tmp/core-sim-local-check all test
```

## Scope
- Fixed-step accumulator policy
- Pause/play/single-step control state
- Max ticks per frame clamp
- Ordered simulation pass execution
- Deterministic per-frame outcome reporting
- Public pass-order validation, pass-outcome initialization, status names, and frame reason bits for host adapters
- UI-free frame summary, reason-name, and stage-timing helpers for diagnostics adapters

## Boundary
`core_sim` owns simulation orchestration semantics only.

It does not own:
- physics equations
- entity/world storage
- scenario formats
- rendering or UI
- platform input
- worker/job/scheduler ownership

Apps provide domain callbacks for simulation passes. `core_sim` decides when and in what order those callbacks run.

## Contract
- `core_sim` is a dependency-light control-plane only. It does not own time providers, schedulers, jobs, workers, wake behavior, trace capture, or retained scene/entity storage.
- `core_sim_loop_advance(...)` accepts only finite non-negative frame dt input. Invalid state, request, policy, or pass-order input returns an invalid frame outcome instead of mutating the loop through host callbacks.
- `core_sim_step_policy_valid(...)` requires a finite positive `fixed_dt_seconds` and a non-zero `max_ticks_per_frame`.
- Paused frames execute no ticks unless `single_step_requested` is armed. A paused single-step consumes exactly one tick and then clears the request.
- Active frames execute deterministic fixed-step ticks in declared pass order until the accumulator falls below `fixed_dt_seconds` or the max-tick clamp is reached.
- Artifact/header/summary/record/stage-timing helpers are pure reporting helpers layered on top of completed outcomes. They do not imply pack, trace, scheduler, or host-runtime ownership.
- This module currently has no separate `ROADMAP.md`; this README is the truth doc for the current hardening pass.

## Dependencies
- C standard library only

Future adapters may layer on:
- `core_time`
- `core_kernel`
- `core_sched`
- `core_jobs`
- `core_workers`
- `core_queue`
- `core_wake`
- `core_trace`

## Status
Legacy control-plane implementation has standalone tests and four proving hosts;
`v0.8.1` additionally qualifies exact model time, independent schedules, typed
exchange metadata and joint progress, plus an optional process-tested reference
host, without new app adoption:
- `gravity_orbit_sim` fixed-step runtime-loop adapter
- `behavior_sim` ordered pass-execution adapter
- `physics_sim` scene-level substep pass network plus 3D solver first-pass shell adapter
- `ray_tracing` progressive/render runtime-frame adapter

## Build

```sh
make -C shared/core/core_sim
```

## Test

```sh
make -C shared/core/core_sim test
```

## Change Notes
- `0.8.1`: T5 optional POSIX reference host and Python process tests for conserved
  source/return coupling, exact observation brackets, atomic checkpoint
  publication and resumed/uninterrupted equivalence. No library API or T1–T4
  implementation changes; this is example/test qualification only.
- `0.8.0`: additive T4 frozen joint plans, complete checkpoint/consumption
  candidate validation, restart metadata checks and proposed accepted-state
  transitions with last-window replay/conflict classification. No durable
  publication, native rollback, automatic retry or app adoption is introduced.
- `0.7.0`: additive T3 channel/record metadata validation, explicit temporal
  sampling admission, pairwise replay/conflict classification and exact interval
  overlap weights. T1/T2 and legacy APIs retain their behavior. No native field
  interpolation, acceptance ledger or application adoption is introduced.
- `0.6.0`: additive T2 independent participant schedules, indexed exchange
  windows/steps and exact observation brackets. T1 and legacy APIs retain their
  behavior. No solver, channel admission or joint-progress runtime is adopted.
- `0.5.0`: additive T1 exact model-time API and standalone rational/limit tests;
  legacy loop APIs remain source-compatible. Header/runtime version now agrees
  with module VERSION (the previous header still reported 0.4.0). Makefile
  supports task-owned `OBJ_DIR` and absolute test-binary paths. No app adoption,
  multi-rate schedule or coupled exchange runtime is claimed.
- `0.4.2`: patch-level pedantic cleanup: the internal FNV-1a hash constants now use typed static constants instead of oversized enum values, removing standards-warning noise without changing runtime behavior.
- `0.4.1`: patch hardening for control-plane edge behavior: finite policy/frame-dt validation is now explicit, stage-timing helpers reject non-finite mark data, and the README now truth-locks helper/lifecycle boundaries and paused/single-step semantics.
- `0.4.0`: additive Step 3 artifact-record helpers: public version string,
  deterministic pass-order hashing, artifact run-header initialization, and
  frame-record extraction from completed frame outcomes. These helpers keep
  trace/data/pack sinks optional and dependency-free.
- `0.3.0`: additive host-adapter examples plus UI-free diagnostics helpers:
  reason-name extraction, frame outcome summaries, and stage timing derivation.
  Optional `core_trace` / `core_data` / `core_pack` adapter contracts are
  documented without adding hard module dependencies.
- Roadmap boundary, 2026-05-03: `core_sim` is now proven across fixed-step,
  entity/group pass-order, solver/substep, and progressive/render-frame host
  shapes. Near-term work should focus on examples, diagnostics vocabulary, and
  optional trace/snapshot/replay adapters, not on moving solvers, entity storage,
  renderer policy, or scenario schemas into this module.
- `0.2.0`: additive adapter hardening with public status names, pass-order validation, pass-outcome initialization, and frame reason bits; `behavior_sim` now uses `core_sim` for ordered stub-pass execution.
- `0.1.0`: initial fixed-step loop state, pause/play/single-step controls, ordered pass execution, max-tick clamp, and frame outcome contract.
- `physics_sim` consumes `core_sim >= 0.2.0` through its vendored shared subtree for a scene-level substep pass network and the 3D solver first-pass shell; solver math, mode hooks, emitter/backend/object operations, scene time, and render/HUD meaning remain app-local callbacks.
- `ray_tracing` consumes `core_sim >= 0.2.0` through its vendored shared subtree for one-tick runtime-frame pass routing (`events`, `update`, `route`, `submit`, `loop_conditions`); progressive accumulation, scene/camera math, renderer submission, and window policy remain app-local.

## References
- `docs/HOST_ADAPTER_EXAMPLES.md`
- `docs/OPTIONAL_TRACE_DATA_PACK_ADAPTERS.md`
- `docs/TRACE_DATA_PACK_STEP3_PLAN.md`
