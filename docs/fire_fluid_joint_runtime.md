# Headless thermal process interfaces

Two bounded Main Edit consumers use the same online receiver interface: the
five-second warm-wall runtime below and the spatial prescribed-shear benchmark
in the final section. Neither enables buoyancy, ignition or installed-session
adoption.

## Warm-wall joint runtime

This Main Edit development path consumes the qualified `wall-refined` solver in
a headless coordinator, with optional separate Thermal receiver process. It evolves
gas temperature and four two-cell Thermal receivers together. It is restricted to the five-second warm-gas, exchange-off
and reversed-temperature fixtures, with prescribed velocity. The bounded online process connection is available; evolving flow, combustion
and ordinary app defaults remain outside this path.

The fixed profile is `wall-refined-v1-n512-cn51200-ex6400`: 4×4×512 gas cells,
scalar step 1/51200 s, one receiver/exchange step per 1/6400 s, conservative CN
normal diffusion, normal-half/wall/normal-half composition. The previously
qualified physics, bounds, conservative limiter and 10% gates are unchanged.
The original demonstrator and runtime call `physics_sim_thermal_step` together;
there is one numerical implementation.

## Build and run

Use `scripts/build_thermal_transport.py --joint-thermal --warm-solid --columns
--core-version 0.3.3 --thermal <frozen-thermal> --shared <frozen-shared> --output
<fresh-build>`. This builds strict and ASan/UBSan executables, references and the
four affected wall tests without cleaning existing output. Inputs must be the
qualified Thermal 0.3.3 fixture; this is not a shared-library importer.

The executable contract is:

```text
probe run|resume MODE CHECKPOINT OBSERVATIONS PACKETS PROFILE END_TICK [RECEIVER_WORKER]
```

MODE 0 heats a cool solid with warm gas, 1 disables exchange, and 2 reverses the
initial temperature difference. END_TICK is an increasing multiple of eight,
no greater than 256000. `run` requires an absent checkpoint; `resume` requires
exact source/fixture/profile identity. A rebuild with changed source deliberately
refuses the old checkpoint. Observation and packet destinations must be absent.
A successful command prints an accepted terminal receipt and independently reads
back its checkpoint. Profile and numeric argument rejection occurs before IO.

The CLI publishes a checkpoint at initial state and at the requested range end.
It admits intermediate steps only in process memory; it does not fsync every
receiver step. After interruption, resume the last published checkpoint. An
unfinished range's exports are speculative and must not be concatenated into an
accepted history. Publish new export paths for the resumed range, and use only
ranges with accepted terminal receipts. No automatic replay to a live receiver
is provided.

## Admission and persistence

`physics_sim_joint_prepare` computes a candidate from the accepted state; exact
available/expected clocks must match the next eight scalar ticks. Admission
checks gas bounds, finite values, fixed receiver configuration, receiver/gas
clocks, signed heat, gas debit, receiver delivery and total energy closure. A
candidate is bound to its base and resulting state digests. Rejection changes
neither accepted state nor output candidate. `physics_sim_joint_accept` commits
the complete gas–solid candidate together, refusing stale or replayed candidates.
Numerical matrices and native gas arrays are scratch, rebuilt from accepted gas
values and prescribed velocity on each new process.

The versioned checkpoint stores explicit binary64 fields and integer clocks,
not ABI structures or pointers: 512 gas rises and their retained correction
values, source ledger, receiver energies, accumulated heat and flux ledgers,
receiver input identities, exact time, generation, profile and source identities.
Receiver configuration and plans are reconstructed from the fixed fixture and
validated before acceptance. Preserving the correction values is essential to
lossless continuation of tiny increments.

Publication uses a nonblocking single-writer lock, expected-state compare-and-
swap, exclusive temporary file, file fsync, rename and parent-directory fsync.
A failure before rename preserves the previous checkpoint. An uncertain result
after rename requires reading the same checkpoint; it must not be retried as a
new transfer. An abandoned exclusive temporary file requires explicit inspection
before removal; publication will not overwrite it. The FNV checksum detects
accidental corruption and is not authentication. Paths and files belong to a
trusted local development workspace. Fsync/rename tests prove the local API
protocol, not survival of physical power loss on every storage system.

## Qualification and remaining limits

Strict, ASan/UBSan and segmented process runs are compared to the retained
qualified five-second warm/off/reversal histories: 501 observations and 32000
packet rows per run. Restarts include scalar ticks 8, 51192, 51200, 51208 and
204800, crossing heater-off at 1 s. Final state and retained corrections must
match uninterrupted continuation exactly. Additional tests inject refused
physics steps, stale candidates/writers, lock contention, corruption, invalid
states with valid checksums, and failures before/after publication.

Exact historical parity preserves the declared physical qualification. It does
not improve the old faint-arrival failures or the additional 26 near-zero
signed-change point discrepancies. Those remain open under their original gates.
The separate process connection is qualified below. The spatial consumer below extends this interface to the retained along-wall
benchmark, before new buoyancy/momentum or combustion feedback.


## Online receiver process (2026-10-20)

Build with the additional `--live-receiver` flag. `probe` and `probe-sanitize`
retain their in-process control when the optional final path is omitted. Supplying
an absolute `receiver-worker` executable path routes every native solid stage to
that process. Use `receiver-worker-sanitize` with the sanitizer coordinator.
Both processes are built from identified source. Caller must verify the worker
binary/input manifest; the local protocol checksum is not authentication.

The new PhysicsSim-private client, wire codec, fixed fixture and worker are thin
adapters around unchanged Thermal `thermal_receiver_stage`. The worker links
only the Thermal/core numerical dependency closure; symbol readback confirms
no gas solve. The coordinator samples gas, computes the unchanged wall exchange,
sends the exact integrated-joule packet plus accepted solid state, and admits
the returned solid candidate after the original normal/gas/energy gates. Solid
temperatures returned from the solve participate in subsequent wall exchange as
in the qualified local benchmark. No new fluid momentum/buoyancy law is enabled.

The fixed v1 protocol serializes explicit bounded fields over inherited pipes:
version, transaction, column, receiver state/ledgers, packet identities/exact
interval/sequence/heat and availability. Replies bind the exact request hash,
returned receiver state and actual Thermal receipt interval, source checkpoint,
payload hash, accepted/allocated joules and time residual. Frames have a maximum
2048-byte payload and finite IO waits. No pointer, plan or ABI structure is sent.
The CLI owns one worker at a time and restores its scoped SIGPIPE handling on
close. EOF, malformed replies and IO failure poison that connection; restart it
before retrying from the last accepted joint state.

A worker stages from the supplied accepted state on every request. It has no
independent accepted frontier or durable heat ledger. Therefore a response lost
after the native solve cannot leave a separately committed solid ahead of gas.
Retrying the same accepted base and packet returns the same candidate. The
coordinator alone commits both participants and prevents stale candidate/replay
acceptance. Restart uses the existing joint checkpoint, including the temperature
corrections; exports from unfinished ranges remain speculative as described above.
This fixed benchmark adapter is not a general network protocol or Thermal GUI/
session bridge.

Frozen Thermal/core0.3.3 is the original numerical control. Current Thermal owner
sources and workspace core0.3.4 are additionally compiled read-only into a separate
worker with their own input/binary manifest. Their physical-quantity wrappers
are compared to the same fixed gas/coordinator baseline; no source import, shared
minimum change or silent checkpoint migration is performed. Current-source proof
must use its recorded hashes, not infer engine identity from a pathname.


To build a separately identified receiver from the current Thermal owner while
keeping an existing coordinator identity fixed, use:

```text
python3 scripts/build_live_thermal_receiver.py --coordinator-identity FRESH_BUILD/identity.json --thermal THERMAL_SOURCE --shared SHARED_SOURCE --output FRESH_RECEIVER_BUILD
```

The helper requires fresh output outside source owners, verifies its adapter
sources match the coordinator, captures actual numerical inputs and engine
version, and builds strict/sanitizer workers. It refuses source drift during
compilation. Select the resulting executable explicitly in the joint CLI;
verify its new manifest separately from the coordinator manifest. This does not
import current sources or migrate a checkpoint. Existing fixture metadata records
the original in-process control; accepted worker-stage receipts identify the
chosen online execution mode.


## Spatial prescribed-shear receiver (2026-10-20)

The existing selected benchmark uses its own fixed receiver family:
`prescribed-shear-v1-nx128-nz256-ex5-6400`. It retains all128native wall strips,
256normal cells, exact6400Hz native/receiver clock, five-tick exchange windows,
1/51200s fine scalar and1/12800s coarse scalar steps. Source-off is tick2560
(0.4s), mesh restriction tick3128(0.48875s), observations every160ticks(0.025s),
and maximum horizon tick3200(0.5s). The1K/s patch, physical face footprints,
0.0625/128m² strip area, capacity50/128J/K per depth, internal conductance2/128W/K,
half-cell/contact resistances, initial/reference300K and max500K remain original.
No spatial response is mapped onto the warm four-strip fixture.

Build into fresh output with:

```text
python3 scripts/build_thermal_transport.py --live-receiver --columns --localized-startup --local-micro-factor 2 --solid-trace --repaired-scalars --shear-normal256 --core-version 0.3.3 --thermal FROZEN_THERMAL --shared FROZEN_SHARED --output FRESH_BUILD
```

Run the existing local control or append the explicit receiver executable:

```text
FRESH_BUILD/probe 128 256 5 3200 10 NEW_PACKETS
FRESH_BUILD/probe 128 256 5 3200 10 NEW_PACKETS ABSOLUTE_RECEIVER_WORKER
FRESH_BUILD/probe 128 256 5 3200 10 NEW_PACKETS ABSOLUTE_RECEIVER_WORKER restart
```

Redirect stdout for observations and stderr for process receipts. The optional
`restart` control replaces the receiver process at host ticks2560,3125and3130,
bracketing source-off and mesh restriction. Child profiles`shear256-320`and
`shear256-3200` retain the matching short/full horizon. The same source-bound
receiver builder above also accepts this spatial coordinator identity, allowing
a separately identified current0.3.4 worker with the frozen0.3.3 local control.

Ten spatial refusal controls include late-column corrupt/mismatched packets,
post-solid and post-scalar rejection and actual child death after64columns have
staged. Accepted gas, receiver and localized scalar states remain unchanged;
subsequent execution retries from them. These stages are private candidates,
not committed heat. Request counts include refused/retried candidate batches;
only the640accepted five-tick windows form the full0.5s history.

Worker startup/next-request idle waiting is separate from response waiting. The
worker can wait60wall-clock seconds while the gas solver computes; frame-body
and parent reply deadlines remain5seconds. The old shared5second idle/reply
budget wrongly terminated a valid spatial run. A six-second idle regression
precedes complete packet replay. This changes no simulation time or tolerance.

### Durable spatial host ranges

The opt-in `--durable-shear` build adds a separate spatial checkpoint codec;
the warm-wall checkpoint schema remains inapplicable. This preserves the same
fixed 128-strip, 256-normal, 6400 Hz, five-tick exchange, 0.5-second benchmark.
No operator, source, coefficient, limiter, bound or physical gate changes.

```text
python3 scripts/build_thermal_transport.py --durable-shear --live-receiver --columns --localized-startup --local-micro-factor 2 --solid-trace --repaired-scalars --shear-normal256 --core-version 0.3.3 --thermal FROZEN_THERMAL --shared FROZEN_SHARED --output FRESH_BUILD
python3 scripts/run_shear_durable.py --build FRESH_BUILD --checkpoint STATE --output NEW_RANGE_EXPORT --mode new --end-tick 200
python3 scripts/run_shear_durable.py --build FRESH_BUILD --checkpoint STATE --output NEXT_RANGE_EXPORT --mode resume --end-tick 2560
```

The launcher verifies the actual host/worker executable hashes against the
build manifest. Its engine binding includes that manifest, both binary hashes
and the spatial profile. The checkpoint also binds compiled source, fixture
and support identities. Keep the exact tools and manifest with a checkpoint;
this is a deliberately strict development format, without migration or
cross-build compatibility. Inputs in the build manifest are provenance;
launch verification checks the retained executables, not mutable source paths.

An explicit little-endian field codec stores native gas energy, accepted fine
or coarse localized scalar values, native/solid clocks, source energy, all 128
receiver dynamic states and cumulative ledgers, replay values and diagnostics.
Geometry, held velocity, material/configuration and solver matrices are rebuilt
from the exact fixed profile. The native gas projection must exactly equal the
localized accepted field; the local wall-response cache is reconstructed.
Checksum, exact length, finite values, temperature bounds, profile, clock,
energy, receiver binding and fine/coarse phase checks precede restoration.
FNV checksums detect accidental corruption; this trusted-local format is not
an authenticated transport for hostile files.

Each range first publishes a pending operation containing the previous accepted
physical state, requested target and operation identity. The host then computes
privately. A lock, expected-file checksum, exclusive temporary file, file fsync,
atomic rename and directory fsync publish the complete accepted snapshot. The
host remains the sole commit owner; the receiver only stages supplied states.
After interruption, replay the SAME pending target from the retained accepted
state. It cannot credit a separately committed receiver ledger twice. A changed
pending target, stale writer, mismatched engine or corrupt file is refused. An
already committed requested end returns success with zero advancement. A
post-rename uncertain publication is read back before continuation.

Exports are speculative until their range receipt says `committed`; the
checkpoint is the accepted frontier. Reusing a committed end does not regenerate
missing history. Preserve the original range directory after uncertain output;
resume to a fresh export directory and reconcile the checkpoint. This provides
physical-state recovery, not a transactional historical database. A receipt
records model end seconds separately from measured wall-clock execution time.
The original terminal `steps` field is the configured 3200-tick horizon; range
acceptance is identified by the checkpoint/receipt, not that field.

Failure controls 1/2 interrupt before write/before rename, 3 forces uncertain
post-rename readback, 4 stops after pending intent, 5 stops after an accepted
in-memory exchange and 6 exits after committed publication before its log.
These are engineering controls, not physical settings. Existing real receiver
SIGKILL controls remain active at startup and at the nonzero 160-tick state;
their completed mask and diagnostics survive host restart.

Physical 10% and reference 0.5% gates are unchanged. Exact restart parity cannot
turn prior faint-point or unresolved-reference results into passes. No automatic
larger grid, longer horizon, buoyancy or ignition is provided.

### Experimental 0.1-second early-tail path

The spatial receiver dispatcher also accepts `shear256-640` for the bounded
0.1 s case. `build_thermal_transport.py --tail-diffusion` uses the new gas
candidate while keeping the same receiver law, geometry, 6400 Hz timebase and
five-tick windows. It is limited to 0.05/0.1 s and cannot be combined with
`--durable-shear`. The retained old checkpoint and tools remain a separate exact
engine; the changed candidate has not inherited that recovery qualification.
See [early-tail method and limits](fire_fluid_early_tail.md).
