# Explicit retained job-pair rollback

scripts/job_pair_recovery.py defaults to read-only planning for a held fixed
job_status.json + output/report.json pair. It reuses the existing bounded storage
readers, strict object decoder and file witnesses; job policy remains app-specific.
Planning takes a cooperative shared operation lock without creating a lock or any
files. It validates the pending schema, fixed native plan and physical job-root
identity, restricts the attempt to .job-pair-PID-SEQUENCE, and admits only known
retained roles and bounded recovery-stage names. Current targets must contain
recognized retained old/new bytes or a legitimate interrupted rollback absence.
Unknown content, linked/special entries, unexplained missing targets, contradictory
receipts and ambiguous resumed slots hold.

The plan inventories complete SHA-256 content and inode/mode/link/size/nanosecond
witnesses of selected files, known absences and directory identities. A final
inventory/witness pass rejects observation drift. Read-only output includes its
own canonical plan_sha256. Rollback requires that exact digest, reacquires an
exclusive native operation lock and replans before mutation. Job-root admission
matches the native generated-storage restriction inside source checkouts and
reaffirms it through ownership; a newly introduced source boundary stops recovery.

Operator commands, from Main Edit:

    python3 -B scripts/job_pair_recovery.py --job-root /absolute/generated/jobs/JOB
    python3 -B scripts/job_pair_recovery.py --job-root /absolute/generated/jobs/JOB --rollback --expected-plan-sha256 DIGEST_FROM_PLAN

Review the entire plan, then use its exact digest. The digest is an observation
binding, not authentication of historical producer bytes. The native v1 journal
still lacks a producer-written cryptographic inventory; corruption predating first
planning is not disproved merely by computing SHA-256 now. Forward promotion is
not permitted by this helper and remains a separate requirement. No held real
user job was recovered in this qualification; only owned control fixtures were
applied. Do not manually delete the hold to bypass recovery.

Rollback records an atomic retained intent binding the journal and old/new content
digests. It prepares complete checked restore stages from retained old copies,
renames any displaced current file to rollback-new-status/report.json, then
restores old bytes or their recorded absences. Original retained copies and failed
new/current evidence remain. Every mutation is preceded by an exact whole-plan
check and followed by an inventory check allowing only its intended paths to
change. The helper syncs affected directories, verifies both restored destinations,
records an atomic rolled-back.json receipt, checks the whole plan and owner again,
then clears only the witnessed hold. It restores byte contents with private file
permissions, not original inode/ownership/timestamps. Direct readers still lack
atomic two-file visibility. Repeating after hold clearance is held as missing
pending evidence rather than applying another rollback.

Interrupted recovery can be replanned and resumed with the new exact digest.
Partially written recovery stages remain as admitted bounded evidence; fresh
exclusive stages are created instead of truncating or deleting them. An existing
intent binds subsequent plans to the initial selected snapshots. Foreign hold
bytes, unexpected files and contradictory states require inspection; the helper
never chooses a new interpretation silently. Power-loss/storage durability and
all write/flush/close boundaries remain unqualified; sampled timeout admission
cannot interrupt a blocked filesystem syscall.

Bounds: selected regular data is at most 1 MiB per file, records at most 1 KiB,
the hold at most 256 bytes, capsule enumeration at most 128 entries, and one
stage-name allocation at most 64 candidates. SHA/identity observation accounts
at most 512 MiB across one operation, with a sampled 120-second check. Separate
bounded decoder and stage readbacks are additional reads; this is not a hard total
I/O budget. Native publication/recovery storage retirement and workflow quotas
remain required.

Verification: 12 recovery methods plus six native pair, 17 metadata, 28 field,
10 path and nine operation-guard methods pass (82 distinct). Ordinary, policy and
bundle integrations pass. Recovery tests use real native interrupted pairs at both
promotion boundaries and all four old-file presence combinations. SIGKILL after
all six rollback rename boundaries replans/resumes; a partial record write retains
its stage and resumes. Tests cover wrong digest, same-byte witness drift, foreign
current bytes, snapshot hardlinks, unknown entries, path/schema escape, lock
contention, unrelated slot drift, protected source paths and source-boundary change.
CLI default read-only, missing-digest rejection and explicit rollback are checked.
The broad run had ten recovery methods; final recovery-only runs add the two
storage-boundary cases. Native production code was unchanged by those additions.

Evidence: data/experiments/lifecycle-validation/20261007-job-pair-recovery retains
source/tests/logs and a native control-pair death/read-plan/rollback example at its
original physical path. Its toy value fields prove metadata control, not CFD
numerical acceptance. The example preserves both old/new copies and displaced new
files. No source or installed/public promotion, commit, package or release changed;
no shared API/version/minimum dependency changed. Recovery on archive-restored
physical roots requires separately reviewed ownership migration; a copied root
cannot reuse the original physical-root journal binding. This packet is outside
independently archived batches. Forward recovery, producer authentication, full
fault coverage, generation readers, worker lifetime/log limits, retirement and
canonical adoption remain open; the broad lifecycle goal remains incomplete.
