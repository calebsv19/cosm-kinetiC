# Native job metadata publication

The detached runner now stages canonical request, status, shared envelope/report,
PID and cancellation metadata instead of truncating the destination. The app-local
adapter reuses the native sidecar's exclusive stage, descriptor witnesses, flush,
atomic rename and failure retention. A new destination remains absent until its
first complete generation publishes; no empty metadata placeholder is allocated.

Existing destinations must be admitted singly-linked regular files. JSON
predecessors must pass the strict bounded object reader; text predecessors are
bounded to 4 KiB. The predecessor's descriptor, inode, size, mode, link count and
nanosecond timestamps are checked during publication. Existing malformed or
changed metadata is held rather than repaired by replacement. Parent directories
are admitted before bundle/report directory creation. Protected, linked, source
and special destinations remain held.

Each metadata parent has an empty `.physics-sim-job-metadata.lock`. The writer
admits its regular/no-link identity and holds a nonblocking exclusive flock through
staging, validation and publication. Contention fails without creating another
stage. Descriptor/path rechecks hold replaced locks and directories. Descriptors
are close-on-exec, and ordinary finish/failure closes them. This serializes a
single write operation among cooperative writers sharing that parent. The outer operation guard in `native_job_operation_guard.md` now serializes
public runner read/update operations across their metadata parents. The per-file
lock alone does not cover those earlier reads or the full worker lifetime. Locks and schema declarations do not authenticate
ownership.

JSON stages must be nonempty strict objects up to 16 MiB; text stages are nonempty
and bounded to 4 KiB. Payload validation, stream errors, flush and sync failures
hold publication. Partial/failed stages remain available for diagnosis. On success
the old mutable generation is replaced; this is not immutable generation history.
The sidecar publisher syncs the staged file and parent namespace. A failure after
rename can leave a published generation with a reported hold; automated rollback
or complete power-loss recovery is not claimed.

Ordinary clean holds the metadata lock and `.headless-sidecar-*.pending` files
even if a mistaken disposable receipt matches them. Retention audit classifies
the lock as an operational job marker and pending stages as retained evidence.
No prune, migration, worker restart or canonical adoption occurs.

Validation: eleven compiled behavior checks cover an absent first destination,
forced-death predecessor preservation, subsequent ordinary publication, competing
writers, eighty complete reader-visible generations, malformed payloads, in-place
and foreign replacement, replaced locks, real file-size I/O failure and linked
metadata. The full affected suite passed 80 checks, including cleanup/retention
classification. All six CLI, Water, scene-cache and detached-runner integration
fixtures passed. Initial admission/test invocation failures remain in the evidence.

Reuse decision: `reuse-deferred` for a new shared API. Inspected `core_data`,
`core_jobs` and `core_workers` own in-memory data/execution, not this storage policy.
`core_io_write_all_atomic` provides ordinary atomic replacement but removes failed
stages and lacks the selected predecessor/lock identity policy. Reuse of the
existing app-native sidecar is adopted. App code owns trusted-local path admission,
job schema and diagnostic retention. No shared module API, VERSION or dependency
adoption changed.

Remaining work includes worker lifetime ownership, multi-file consistency,
append-log ownership and bounds, authenticated process identity, forced-death
reconciliation and recovery. Cooperative checks do not exclude uncooperative
namespace races or guarantee hard syscall/resource bounds. CFD physical accuracy,
installed-product freshness and public release state are separate.

```sh
make test-native-job-file-atomic
```

Evidence: `data/experiments/lifecycle-validation/20261007-native-job-metadata-publication`.
This packet is outside the prepared independent archive snapshot.
