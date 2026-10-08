# Retained job metadata pair and durable publication holds

The fixed job_status.json + output/report.json publication now retains copies of
both old and both prepared new representations before promoting either file.
Missing predecessors are recorded independently in the plan. Each copy is at
most 1 MiB, streamed with an 8 KiB copy buffer and checked against its source and
full descriptor/entry witness. Source copies, plan and their parent directories
are flushed before an exclusive durable publication hold is created. Copies
retain exact bytes, not original inode identity or complete filesystem metadata.

The publisher requires an active matching job operation guard and the fixed two
paths/parent relationship. Exclusive 0700 .job-pair-PID-SEQUENCE capsules retain
old-status.json, old-report.json, new-status.json, new-report.json and plan.json.
The plan uses fixed relative paths and records the physical job-root identity.
The root hold .physics-sim-job-publication.pending names the retained attempt.
Partial preparation attempts are also retained. No recursive deletion or evidence
pruning is introduced. Stage/capsule creation before a hold can leave orphan
attempts while the untouched predecessors remain usable.

All retained entries, stages and predecessor witnesses are checked before first
promotion and again before second promotion. After both writes, current targets
are compared completely with retained new copies under descriptor/named witnesses,
output/capsule directory identities are checked, and an exclusive completed.json
receipt is flushed. Retained plan/copies/targets and the owned hold are rechecked
before removing only the matching hold and flushing the job root. Changed or
foreign hold bytes remain preserved. This is cooperative local publication;
conditional unlink is not a kernel atomic compare-and-unlink operation against
arbitrary noncooperating adversaries.

New runner operation guards reject any pending hold entry, including empty,
directory, dangling/link and hardlink forms. No parsing or clearing of unknown
holds is attempted at operation admission. Therefore a process death between the
two promotions, a promotion/flush failure or changed evidence leaves later status,
cancel and other guarded operations held rather than admitting a mixed pair as
ordinary ready state. The active publishing guard remains valid through its own
hold; new operations acquire/check the operation lock and then test absence.

Verification: six native pair methods, 17 metadata methods, 28 field methods,
10 path methods and nine operation-guard methods pass (70 distinct); ordinary,
policy and bundle integrations pass. Native fault compilation interposes renameat
only in the sidecar object. The probe tests SIGKILL after each of the two successful
promotions and injected EIO before the second promotion: old/new copies and the
hold remain and retry admission fails. Another fault changes the hold after first
promotion; foreign bytes remain and second promotion is blocked. Normal and
initial-absent publication retain their evidence and clear the hold. Actual-runner
status/cancel probes cover four unknown hold forms without status or cancel-flag
mutation. These cases do not qualify all write/fsync/close or durable boundaries.
The final actual-runner integration run followed the final plan/hold checks. Two
comment-only corrections afterward describe the implementation; exact reverse
replacement proved no other source change in that correction.

## Still required

This is retained fail-closed publication, not an atomic two-file visibility
transaction or completed recovery system. Explicit read-only planning and
digest-bound rollback/forward recovery remain required, with interruption/resume,
whole-plan drift and absent predecessor coverage. Do not manually remove a hold
or let ordinary status retry infer a recovery direction. Copied file bytes are
retained, but no durable cryptographic inventory/authenticated producer binding
has been added. Direct readers bypassing the guarded runner can observe mixed
generations. Clearing a hold followed by a directory fsync failure can report an
error with a coherent pair and retained completion evidence; recovery must account
for durable uncertainty at that boundary.

Per-file/fixed-slot byte bounds do not impose hard I/O time limits or aggregate
retained-storage quotas. Every successful refresh can retain another capsule;
archive-backed retirement is required. Cancellation flags, startup/pid transitions,
worker lifetime/log/process ownership and broad lifecycle requirements remain
incomplete. No automatic restoration, promotion, pruning or canonical adoption
is implied by passing this scoped qualification.

Reuse-adopted: existing app sidecar publisher/strict JSON reader/metadata ownership
and already-linked core report meaning. Fixed job-pair policy stays in the app;
no shared module/API/version or minimum dependency changed. No numerical algorithm,
commit, package, installation or release changed. Evidence:
data/experiments/lifecycle-validation/20261007-job-pair-hold. This packet is outside
independently archived batches.
