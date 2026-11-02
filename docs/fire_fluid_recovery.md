## Completed recovery study and current diagnostic boundary

The four-case12.8s recovery matrix is complete and independently audited.
Both continuous cooling and5%half-dt development screens pass; half-dt curve
change is0.04419%. Grid16→32change remains about25%. Wind-off retains97.43%of
actual source heat in gas; its object is still warming at the horizon. Channel
Ywalls and top are closed,so this is not an all-sided open-reservoir case.
Full scientific report and native-field views are retained under the recovery
artifact root. A separately authorized overnight matrix now tests matching
.2s32³/64³,exchange timing and explicit broader controls; its results are pending.

# Optional Fire-to-Fluid delivery cutoff

The source-development Framework accepts an optional top-level configuration:

```json
{"source_delivery":{"schema":"fire_fluid_delivery_cutoff/v1","active_windows":4}}
```

`active_windows` must be an integer from1through windows−1. Delivery is active
for zero-based windows below this count and zero afterward. This is a whole
exchange-window delivery gate. Native Fire continues its ordinary evolution
with accepted-prior gas wind. Original Fire frames and producer digests remain
authoritative and unchanged. The gate does not extinguish combustion.

Each accepted post-initial checkpoint records `evidence.source_delivery`, schema
`fire_fluid_delivery_receipt/v1`, window, source frame digest, active flag,
disposition, and available/delivered/diverted dictionaries. Each dictionary has
`energy_transferred_j` and `smoke_transferred_kg`. Available equals delivered
plus diverted. Closed-gate output is explicitly outside the Fluid subsystem;
it is not discarded from the ledger or counted as receiver input. Native gas
input must plateau exactly after cutoff. Geometry admission still runs when
the gate is closed. Omitted configuration uses the original delivery path.

The cutoff config and module bytes join the existing source binding. Receipt
contents are recomputed during independent journal reopen. Existing joint
transaction, fault/retry, duplicate/stale operation, object checkpoint and
accepted-prior feedback rules apply. Historical bindings are not rewritten.

`coupling.recovery_cases.configuration(workers,name)` defines the four bounded
native Fire/Fluid cases:16³continuous,32³continuous,32³half gas timestep, and
32³wind-off at2.4s. All use32×.4s exchanges, delivery cutoff1.6s, original17×17
Fire and unchanged authored source/object calibration. The12.8s horizon is
short compared with the80s nominal object time constant. No closed-gate output
is used to claim native Fire extinction or material calibration.

Tests:`tests/test_source_delivery.py`. The local numerical matrix is in progress;
see `current_truth.md` for precise completed and pending evidence. Both16³and32³continuous cases show early object cooling after a delayed peak;
half-timestep and wind-off sensitivity comparisons remain pending. The implementation is uncommitted Main Edit headless
source behavior, not installed app or worker activation.
