# Detached native job path admission

Main Edit admits the jobs root before constructing or reading a job slot. The
existing native generated-storage policy normalizes observed macOS aliases and
holds other links, protected/broad storage and checkout source paths. Explicit
relative roots are now represented by their admitted absolute spelling.

Local job IDs are 1–95 ASCII characters: the first is alphanumeric; later
characters may also be `.`, `_` or `-`. Directory separators, leading dots,
whitespace, non-ASCII and oversized IDs are held. The local filesystem restriction
is stricter than the shared envelope's bounded string; the shared schema is not
changed. Generated IDs and supported bundle IDs remain compatible. Constructed
paths also reserve native headless path capacity before allocation.

The job directory, fixed metadata parents and ten file slots are checked before
submit/status/cancel effects. Existing slots must be singly-linked regular files.
Linked directories/leaves, special files and hardlinks are held. Loaded status
records must match the selected job ID and its fixed progress, summary, stdout
and stderr paths before refresh/cancel. Malformed or redirected status is held
rather than printed or followed. Requests/status declare `operational_job` for
conservative clean classification; this is not ownership or pruning proof.

Native runner `status` refreshes and may write status/report data; it is not the
read-only top-level status/doctor command. Cancellation remains cooperative via
a job-local flag. The only `kill` use checks liveness with signal zero; no PID-
based termination is introduced. PID liveness does not authenticate a worker.

These are cooperative admission controls. Native metadata parsing now uses the
bounded strict reader in `native_job_json_lifecycle.md`; a complete field schema
and authenticated job identity remain open. Status,
request, report, PID and cancel publication now use staged per-file replacement
and witnesses in `native_job_metadata_publication.md`. Log writes, job-wide
read/update coordination and forced-death recovery remain open.
Request/output provenance, complete process identity, multi-file consistency,
in-place external edits and uncooperative races remain open. Existing records
with incompatible IDs/path spellings stay held for owner reconciliation; no
migration, deletion, restart or automatic adoption occurs.

Validation: seven actual-runner admission tests cover escaping and oversized IDs,
all ten linked slots, linked roots/jobs, FIFO/hardlinks, protected/source storage
and redirected references to an external FIFO. Fifty-five affected native,
sidecar, atomic, clean and retention checks passed together. All six supported
headless/job-runner integration fixtures passed, including ordinary overwrite,
cancellation, stall observation and bundle reports. Source builds passed without
new warnings. No solver, canonical source, installed/public product or independent
archive changed.

Local proof:

```sh
make BUILD_DIR=build/<fresh-profile> test-native-job-paths
```

Evidence: `data/experiments/lifecycle-validation/20261007-native-job-path-admission`.
This newer packet is outside the prepared independent backup.
