# Job report predecessor admission

Actual-runner before/after probes demonstrated a second publication-order gap:
a malformed regular `output/report.json` was admitted as a path but rejected by
its JSON writer only after `job_status.json` changed. Status and cancellation
both returned errors; a subsequent unchanged status call succeeded with the
malformed report still present. Existing directory, symlink, hardlink and
non-directory-parent cases already held before status mutation; those controls
must not be described as previously accepted.

Refresh and persistence now require an existing report to pass the reused
bounded strict object reader before status mutation. Refresh checks this even
when no metadata change is needed, closing the repeat-read bypass. Invalid
predecessors remain in place; neither command repairs, truncates or deletes them.
Missing legacy reports remain permitted. This admits JSON representation, not
report schema, job identity, artifact integrity or common generation provenance.
The path graph and strict reader retain their existing nofollow/single-link and
bounded-read policies. There is no new shared semantic API or dependency version.

Verification: 26 field methods, 10 path methods and 9 operation-guard methods
pass (45 distinct), plus ordinary, policy and bundle integrations. The new
regression covers five invalid report encodings with both changed and unchanged
refreshes, then status/cancel/status; all preserve exact status/report bytes and
create no cancel flag. A separate ten-case boundary probe records status and
cancel for malformed/directory/symlink/hardlink/non-directory-parent reports.
Only the malformed cases failed the preservation invariant before this repair;
all ten hold afterward. Outside sentinel bytes remained unchanged throughout.
The probe fixtures use valid request paths and resolved temp roots; preliminary
unresolved-path/omitted-request fixtures were not acceptance evidence.

## Required coordinated publication work

This is predecessor preflight, not a two-file transaction. A later report I/O or
namespace failure can still follow status publication. The operation guard
excludes cooperative concurrent runner operations but creates no durable pending
hold; the metadata locks are acquired by each single-file writer separately.
`physics_sim_job_file_finish` validates, flushes and publishes one file, closes
its descriptors and releases its lock. Its pending-file retention does not
restore the displaced status predecessor or bind it to the report.

The complete replacement must stage and admit both new representations before
predecessor displacement; retain both original contents or their recorded
absences; persist a fixed two-slot plan and pending hold; recheck the whole plan
and each slot at publication boundaries; flush every transition; and make
readers refuse an unfinished transaction. Recovery must be explicit and bound
to the retained plan, preserve failed new output, and prove either restored
predecessors or a coherent completed generation before clearing the hold.
Reversing write order or retrying a partial operation is insufficient. Direct
readers require a generation contract or an explicit admission/hold protocol.
Cancellation flag creation is an additional durable transition needing its own
coherence policy; startup/pid publication also needs coverage.

Required fault coverage includes each stage creation/write/flush/close,
predecessor retention, both promotions, directory flush and hold finalization;
forced death at each durable boundary; recovery interruption/resume; namespace
and same-byte replacement; absent predecessors; schema rejection; cooperative
contention; and unchanged status after a held transaction. Neither these tests
nor predecessor preflight claim that qualification complete.

Evidence: `data/experiments/lifecycle-validation/20261007-job-report-predecessor`.
No solver, commit, canonical adoption, installed product or release changed.
Worker lifetime/log quotas, archive retirement and the broad lifecycle objective
remain open. This packet is outside independently archived batches.
