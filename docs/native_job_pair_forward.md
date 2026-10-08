# Explicit verified job-pair forward recovery

The read-only planner now reports verified producer inventory, selected recovery
direction and allowed rollback/forward choices. Forward recovery requires a v2
producer-written digest inventory and the exact current read-only plan digest.
Legacy v1 journals remain explicitly limited to their existing rollback contract;
even an already-new legacy current pair cannot use forward to clear a hold.

An explicit forward installs the exact recorded new status/report bytes. It uses
the existing owned, bounded, whole-plan checked retained recovery engine. Its
atomic forward-intent.json binds the same journal/old/new digests. Each replaced
current file is renamed to forward-old-status/report.json; complete private
restore stages are copied from the retained new snapshots and promoted to the
fixed destinations. All original old/new snapshots and partial/failed stages
remain. Both final targets must match their new snapshot bytes before an atomic
forwarded.json receipt is flushed and only the witnessed hold is removed.

Committed forward and rollback directions are mutually exclusive. A direction
is selected by its durable intent; conflicting intents, displacement without
intent and contradictory terminal receipts hold. After interruption, replan
and resume the same direction with its new exact digest. A complete native
publication receipt does not substitute for explicit recovery direction review
when its hold remains. Direct readers still see separate file promotions; no
atomic two-file visibility or authenticated producer identity is claimed.

Trusted-local Main Edit commands:

    python3 -B scripts/job_pair_recovery.py --job-root /absolute/generated/jobs/JOB
    python3 -B scripts/job_pair_recovery.py --job-root /absolute/generated/jobs/JOB --forward --expected-plan-sha256 DIGEST_FROM_PLAN

Use the full reviewed read-only plan's exact digest. --forward and --rollback
are mutually exclusive; no flag defaults to planning. Forward is a metadata
publication decision. Worker restart, execution continuity and fresh outcome /
artifact provenance checks remain separate requirements. Only owned toy-control
fixtures were forwarded here; no real user job was recovered. A cleared hold
ends the recovery session; repeating without pending evidence holds rather than
silently reapplying.

Verification: ten forward methods, 14 existing recovery methods, seven native
pair methods, 17 metadata methods, 28 field methods, ten path methods and nine
operation-guard methods pass (95 distinct), plus ordinary, policy and bundle
integrations. A new interposed native probe kills before first promotion after
retained plan/hold creation; existing native deaths after both promotions remain
covered. Forward tests exercise all three native publication states and all four
predecessor-presence combinations. SIGKILL after all six forward rename boundaries
replans/resumes; intent direction cannot switch; partial intent stages remain;
legacy, wrong digest, unrelated slot drift and unexplained absence hold. CLI
read-only default, required digest and mutually exclusive modes are covered.
All existing rollback and producer-digest tests remain green. The native probe
hook is test-only; no native production publisher/solver source changed.

The same per-file/record/enumeration/stage allocation and sampled observation
bounds apply as rollback. Every mutation rechecks the exact whole plan and owner,
then permits only intended paths to change. No automatic deletion, pruning,
worker launch/signal, package or release is part of this operation. Hard I/O /
aggregate retained-storage limits and all power-loss/durable-boundary faults
remain incomplete.

Reuse-adopted: the existing PhysicsSim retained recovery ownership, storage reads,
strict JSON, SHA/witness planning, atomic retained record and staged restoration
mechanisms serve both directions. Fixed job-pair purpose stays app-owned; no
shared API/version/minimum dependency/vendor pin changed. Forward adds no new
hash implementation. No commit, canonical adoption, installation or release
changed. Evidence: data/experiments/lifecycle-validation/20261007-job-pair-forward
includes source/tests/logs and a retained native pre-promotion death / read-plan /
forward example preserving old/new copies and displaced old files. The toy value
fields prove metadata control, not CFD numerical results. This packet is outside
independently archived batches. Reader generations, signed producer identity,
full fault/platform qualification, worker lifecycle, storage retirement and
canonical adoption remain open; the broad lifecycle goal remains incomplete.
