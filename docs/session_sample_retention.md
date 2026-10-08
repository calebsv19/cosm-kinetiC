# Local session sample retention

Main Edit preserves retired live-sample diagnostics before removing their active
copies. The rolling window remains 32 active identities/results, with at most
eight pending requests. A pending request is never deleted just because it is
old. A full queue returns `sample_queue_full`; it requires worker progress or
owner reconciliation, rather than automatic age-based removal.

Completed samples leaving the active window are copied into
`runs/<run_id>/sample_history/<request_id>/`. The history contains exact original
request bytes, result bytes when available, and an operational-job receipt with
sizes and SHA-256 hashes. Exclusive creation, readback and file/directory flushes precede active-file
removal. Mismatched, unknown, symlinked or oversized history stays held. A partial
copy can resume only with matching active inputs; a completed history receipt
supports retry after interrupted active-file removal. History is not pruned.

A retry of a retained request ID verifies history and returns its original result.
A changed payload conflicts with that ID, even after the active window rolls.
Terminal `run_result` inventories retained histories as well as active samples.
A damaged or unknown history prevents a clean result claim.

Each run admits up to 4,096 sample requests across active identities and retained
history. This limit is distinct from the 4,096 spatial samples per preview.
At the request limit, new IDs are held; matching-ID reads remain available. The
owner must retain/retire the run through its lifecycle or select a new run. This
change favors preservation and bounded storage over silently deleting diagnostic
history. Request files are bounded at 64 KiB, results at 4 MiB, receipts at 64 KiB;
history enumeration and new-request counting are bounded. These are local
cooperative bounds, not a hard filesystem snapshot or service-wide resource proof.

The service capability response advertises `sample_history_retained` and
`max_sample_requests`. The macOS source packaging recipe already includes all
`agent_session/*.py` modules; no package build or installed update occurred here.
This is an operational retention repair, not a CFD numerical-model change.
Remaining service root/worker supervision, arbitrary historical writers and exact
run retirement/archive eligibility still require their own audit.

Validation uses temporary runs. Focused checks cover exact bytes, pending age,
failed copy, interrupted removal, predecessor conflict, symlinks, file/count
bounds, retained-ID retry and full pending queue. The existing real-worker rolling
sample test also checks retained histories, ID conflict and terminal artifact
inclusion. Worker acceptance remains specific to its tested existing binary.

Final checks: ten focused/control-only tests and nine existing local-session
regressions passed (19 total), including history-directory flush failure.
Existing worker SHA-256: `c94730435a80ff254590ba33b8eab4f00fbb6c5adb98c736dc0432ff718a5c34`.
The worker was not rebuilt or replaced. Source lifecycle evidence is retained at
`data/experiments/lifecycle-validation/20261007-session-sample-preservation`.
