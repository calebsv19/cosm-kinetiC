# Transactional prescribed-flow source consumption

The implemented next boundary is `physics_sim_prescribed_flow_coupled_checkpoint/v1`.
It restores the qualified body-free passive model by replaying the exact retained
native step history under pinned worker bytes. Velocity is explicitly prescribed
and constant; this is not a serialized/restartable native transient CFD solver.

`python3 -B scripts/coupled_passive.py --config CONFIG --store STORE init|inspect|admit|step`
uses one SQLite database (`coupled.sqlite3`) with FULL synchronous publication.
CONFIG contains exact `policy`, `properties`, `velocity_m_s`, `worker` keys. Policy
and native property/grid bounds are validated before initialization. This first
receiver is all-fluid, zero-origin, starts at source tick zero and uses one source
branch. Worker and receiving-adapter bytes, policy, properties and prescribed velocity are immutable. Continuation requires the pinned bytes; upgrades need an explicit future migration.

Admit accepts frame/bundle semantic validation, provenance, config/calibration,
geometry, contiguous sequence/ticks and source-versus-receiving clock checks.
Equal source replay preserves its original admission receipt; changed content
conflicts. Admission itself is not physical consumption.

Step requires expected revision, unique operation ID,dt<=1s and source mode:
`--revision 0 --operation-id first --dt .02`. Required mode splits across already
admitted source boundaries, conservatively allocates each covered fraction and
retains all consumption ranges in the checkpoint. It cannot advance through a
source gap. `--source-mode none` allows source-free transport only after every
admitted interval is finished; it cannot bypass a queued source.

State consists of original initial fields/properties, every accepted scalar step,
face velocity inputs, native field/budget result, consumption ranges/source digests,
parent checkpoint digest, revision and absolute source clock. The remainder is
unconsumed coverage of the retained admitted frame relative to the accepted clock.
The database retains complete source frames. A candidate native replay validates
all sums and derived quantities before checkpoint, operation receipt and head
revision publish in the same SQLite transaction. Worker/timeout/exception/failed
insert leaves accepted history untouched. An equal operation replay returns its
original receipt even after later accepted steps. Conflicting IDs or stale expected
revisions reject. Database copies after writers close can reopen under the same
binding; JSON checkpoint exports are readback/provenance, not a standalone import API.

Limits:64 accepted revisions,256 source events,128MiB conservative database admission
estimate; retained native history keeps the existing256-step/64MiB request/100-million
subcycle-cell/120s execution limits. Replay computation grows with retained history;
this correctness-first bounded path is not a high-rate streaming or distributed
solver checkpoint. It does not promise active-worker cancellation/recovery APIs.

Five focused controls cover partial-interval reopen, equal retry/no duplicate
injection, full budgets, source-free transport, unavailable/bypassed source, stale
revisions/conflict, worker-failure rollback, injected SQL failure and exact replay
against uninterrupted accepted step history, cross-event partial consumption, copied-database restore and immutable property binding. Native source CLI acceptance admits
two Fire bundles, advances .02/.03/.02/.03s in separate processes, retries every
operation and exports revision4 at0.1s with reconciled89624.811988J and0.011949975kg.

GrowthSim's run_coupled_scene.py links the readback to the fire-surface/VF3D carrier.
Existing admission-only journals and original momentum/dye solvers are unchanged.
Next remaining physics work is native evolving-velocity histories with joint
candidate rollback, open boundary scalar flux budgets and buoyancy qualification.
These require additional numerical cases and are not provided by SQLite persistence.
