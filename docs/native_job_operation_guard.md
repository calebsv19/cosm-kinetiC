# Native job operation ownership

Submission, status refresh/display and cancellation now hold a job-level guard
across their cooperative metadata read/update sequence. Submission exclusively
creates its job directory using `mkdir`, then acquires the guard before staging,
spawning and recording launch state. A colliding or interrupted slot remains
held rather than being reused. Status/cancel preserve missing-file diagnostics;
existing-job reads occur only after guard admission. Refresh publication errors
are now propagated, preventing cancellation from continuing after a held refresh.

The guard pins the job directory, its parent and an empty singly-linked regular
`.physics-sim-job-operation.lock`. A nonblocking exclusive flock excludes another
cooperative operation on the same job. Sibling jobs can proceed independently.
Root, parent and lock descriptor/path identities are checked during the operation.
The active thread-local guard is checked by metadata staging/publication; writes
must remain under that selected job. A changed namespace holds later effects.
Bundle/report parent creation also checks the active guard before effects.

Normal completion and failure release the descriptors. Close-on-exec avoids
carrying the operation lock into the worker executable; the detached child may
briefly inherit it between fork and exec. Nested admission cannot overwrite the
existing guard. Forced process death releases the kernel lock but leaves its
marker and interrupted job state for diagnosis. Ordinary clean holds this marker
regardless of a mistaken disposable receipt, and retention audit identifies it
as operational state.

This owns a public runner operation, not the whole simulation lifetime. Native
worker progress/summary publication still has its own sidecar owner. Separate
status/report files remain separate atomic generations; external readers do not
receive a coherent multi-file snapshot. Legacy helpers or external programs that
ignore the guard are not serialized. Path witnesses do not exclude every
uncooperative namespace race and do not authenticate job/process ownership.
No automatic adoption, restart, recovery or deletion is introduced.

Validation: nine compiled/actual-runner behavior checks cover same-job contention,
sibling independence, process death and subsequent admission, descriptor release
at exec, root/parent/lock replacement, linked/special/hardlinked/nonempty locks,
nested admission, status/cancel holds before reading and refresh-failure
propagation before cancellation. The final affected suite passed 89 checks.
Both rebuilt source binaries passed all six supported headless/job-runner
integration fixtures. Initial build-contention and diagnostic failures remain in
the retained logs; final proofs use completed final builds.

The guard extends the app-local storage policy and reuses the established native
sidecar publication adapter. No shared module APIs, VERSION or adoption changed.
Parent-admitted exclusive log descriptors and checked child handoff are now
covered in `native_job_log_handoff.md`. Remaining work: worker lifetime/log bounds
and resource limits, authenticated process
identity, multi-file recovery and forced-death reconciliation, full writer audit,
archive-backed retirement and canonical adoption. Numerical accuracy and installed
or public product qualification remain separate.

```sh
make BUILD_DIR=build/<fresh-profile> test-native-job-operation-guard
```

Evidence: `data/experiments/lifecycle-validation/20261007-native-job-operation-ownership`.
This packet is outside the prepared independent archive snapshot.
