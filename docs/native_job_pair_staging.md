# Complete status/report staging before publication

Production persist_job_state now prepares both job_status.json and output/report.json
before replacing either destination. A real report-lock contention probe against
the previous runner demonstrated status replacement despite command failure;
releasing the lock and reading status then succeeded with the old empty report
still present. The regression now holds both predecessors and retries with a
coherent local completed/shared succeeded result after the lock is released.

The status and report serializers retain their exact previous emission bodies.
They write to caller-owned streams without flushing, closing or replacing paths.
The report serializer reuses core_headless_job_report_validate for the report and
its supplied artifact array/count; purpose-specific staging remains app-owned.
The standalone writers reuse these serializers and the existing finish lifecycle.

Persistence builds/admit the report, admits its predecessor, ensures the two
parents, begins/writes/prepares status, then begins/writes/prepares report. Both
metadata locks and all stage/predecessor descriptors remain owned through a
whole-pair ready check. Preparation failure consumes its stream; every other
owned stream is explicitly discarded, retaining pending evidence. The ready API
rechecks prepared stage, stream errors, predecessor and cooperative guard/lock
without publication or consumption. Both files receive complete stage/size/JSON,
flush/fsync and witness checks before the first status replacement. The existing
publish helper additionally rechecks each slot at its own promotion boundary.

This closes ordinary report stage acquisition/serialization/validation/flush
failures occurring only after status publication. The dedicated fault qualification
here is report-lock contention, not every possible write/fsync/close failure.
Both parent directories and metadata lock/pending files can be created during
preparation; preservation refers to exact destination bytes/absence, not zero
filesystem mutation. Pending stages remain inspectable; retries still have no
pair-level durable hold.

## Remaining transaction requirement

Promotions are still sequential. A failure after status promotion can leave a
mixed pair. No original-status retention, durable transaction plan or pending
hold, forced-death recovery, explicit restoration or immutable-generation reader
contract has been introduced. Whole-plan checks are in-memory consistency checks,
not cryptographic source authentication or atomic visibility. Complete retained
pair recovery is required next; this staging change cannot stand in for it.
Cancellation flags and startup/pid state need their own coupled outcome coverage.

Verification: 17 metadata methods, 27 field methods, 10 path methods and 9
operation-guard methods pass (63 distinct), plus ordinary, policy and bundle
integrations. The prior runner fails the new actual-runner regression at both
status/cancel preservation assertions and post-contention report coherence. An
initial current run passed metadata checks but stopped on a test expecting local
completed in the shared report. Correcting that fixture expectation to the
existing succeeded label was followed by the authoritative runner/integration
run; no production code changed for that repair. All logs are retained. The exact
status/report emission-body comparison against the previous sealed sources passes.

Reuse-adopted: existing app native sidecar, job-file ownership and strict JSON
reader plus already linked core report validation. No shared module/API/version,
minimum dependency, numerical algorithm, commit, canonical adoption, package,
installation or release changed. Broader lifecycle and worker/resource/retirement
requirements remain incomplete. Evidence:
data/experiments/lifecycle-validation/20261007-job-pair-staging. This packet is
outside independently archived batches.
