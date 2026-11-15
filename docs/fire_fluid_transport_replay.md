Current evidence: the four3.2s fixed-source/prescribed-flow controls pass68checkpoints,12native snapshots and65aggregate checks. Common-reference32/64object-curve sensitivity remains19.29%; this is not convergence or a momentum/pressure solution. Controlled timing now passes159checkpoints/28native snapshots and73aggregate checks. Open-domain forcing and eight live comparisons are accepted (264 checkpoints, 48 native snapshots, 94 aggregate checks).

# Retained Fire pulse and fixed-velocity diagnostics

`coupling.source_replay.extract(case)` reads the delivered prefix of a completed,
independently reopened Fire–Fluid case. It preserves source frames, calibration,
producer identity and exact native tick intervals. `Replay(pulse, receiver)`
uses the ordinary clipped nodal-area mapping and physical injection depth;
`steps(first, count, Fraction(dt))` allocates overlap in physical time. There is
no live return channel and no heat normalization applied to the source.

`coupling.transport_replay.ReplayRun` persists native Fluid results in a fresh
SQLite journal. Native acceptance, gas/object and smoke budgets, worker/input
identity and exact clocks remain checked. An explicit audit reopens every
checkpoint and verifies its chain. Existing case roots are never overwritten.

The separate diagnostic worker accepts `diagnostic_transport` equal to
`fixed_mac_velocity/v1`. It requires a fixed prescribed MAC reference, disabled
buoyancy, an inert box, constant boundary command and quiescent momentum ledger.
It reuses native scalar transport and object exchange while holding velocity
exactly fixed. It does not evolve momentum or solve pressure. Ordinary workers
reject that diagnostic mode; ordinary channel start-from-rest rules remain.

Tests cover temporal/physical source conservation, exact legacy allocation,
conservative nested-grid restriction, explicit admission and immutability.
Strict/sanitizer and packet native controls pass. The full32³/64³ comparison
campaign is accepted; these controls do not establish spatial convergence,
measured wall coefficients or hot-fire qualification. See `current_truth.md`
for broader development status. No default runtime or installed-app activation.

## Controlled timing diagnostics (accepted source-development slice)

`coupling.source_timing.native_tick_pulse(case)` derives one native Fire-tick
frame at a time on a separate source fork under recorded donor feedback.
Original native window endpoints and per-node extensive sums must match; no
producer frames or calibration are rewritten. The explicit native-tick pulse
profile admits at most128frames; the ordinary retained-pulse profile keeps its
32frame bound. Temporal partitioning can change the last accumulated bit.

`Replay(..., deposition_interval=Fraction(1,5))` deliberately groups whole
producer frames and spreads their amounts uniformly over that group. It refuses
a period that cuts a producer frame or fails to divide the pulse horizon.
This is a deposition approximation, not finer emission data or a new burn law.

`fire_fluid_timing.py --workers workers.json --coarse-pulse pulse.json
--tick-pulse tick-pulse.json --output <fresh-root> --budget-root <task-root>`
runs six serial32³controls: host100/200/400, emission020/200, live100. Native
clocks, physical source/object,3.2s horizon and1.6s delivery cutoff stay fixed.
The CLI verifies retained worker hashes and the CodeWork active-consumer helper
before admission. Failed roots remain; successful native staging retires only
after accepted SQLite publication and consumer checks. No existing root reuse.

Fifteen targeted timing/recipe/legacy replay tests pass. Native comparisons,
full timing audit and the 73-check aggregate gate pass. Open-domain gust forcing
and its bounded live campaign are accepted separately. No default/app activation
is implied.

The final timing gate covers seven32³histories,159checkpoints,28native snapshots
and32live source-feedback windows. Host0.1/0.2/0.4sreplay DATA match exactly at
nine shared checkpoints. Warming-curve relativeL2 is0.041245% for native0.02versus
uniform0.2sdeposition,0.031246% for uniform0.1versus0.2s, and3.782918% for live
versus retained-feedback histories with matching0.1sdeposition. Live feedback
delivers4.192896%less actual heat in that comparison; no dose normalization is
applied. These nonlinear controls are not an additive solver-error decomposition
or a convergence/material qualification. The selected-case CLI includes
`--case emission100`; completed roots are never reused.

[Open-domain forcing](fire_fluid_open_weather.md) is a separate opt-in native
feature with distinct mechanical-work and observed air receipts. Its eight-case live
coupled campaign passes 264 checkpoints, 48 native snapshots and 94 aggregate
checks; installed/default behavior is unchanged.

## Saved-carrier MC crossover · 2026-11-04

The explicit fixed_mac_velocity/v1 diagnostic now accepts mc_ssprk2/v1.
A frozen-worker build can also hold a saved ambient-reservoir MAC field without
an inlet schedule. All normal faces stay fixed, the native divergence guard
remains 1e−8 s−1, and host validation requires exact velocity identity on every
result and restore. A supplied inlet schedule retains its existing constant
boundary checks. This is a source-only numerical diagnostic selected by a
separate worker build and request identity.

The gap crossover starts two fresh 64³ passive-smoke fixtures from the same
saved smoke field. The two final-time carrier fields are the original 64³
field and an area-restricted 128³ field. Each advances 20 native 5 ms steps,
with zero sources, disabled buoyancy, no object exchange and no momentum
advance. Full versus fresh-process 10+10-step continuation must match exactly.
Optimized/sanitizer two-step prefixes must also match all native state data.
Global source/boundary smoke ledgers and fixed velocity identity are checked
independently. Regional inventory differences measure net retention, with all
region faces contributing. Separate gross gap-entry flux is not retained.
