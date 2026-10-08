# Native detached startup observation

The native runner no longer treats fork success alone as its launch observation.
It allocates a close-on-exec startup pipe before fork. The child reports a small
phase/errno record on session setup, log redirection or exec failure. The parent
waits at most ten seconds for channel data or closure using a monotonic deadline,
nonblocking reads and poll. The startup pipe is separate from retained stdout and
stderr and remains above standard descriptor numbers.

Channel closure without a reported setup error is the accepted exec observation.
It is not authenticated executable identity, application-ready acknowledgment,
worker lifetime ownership or numerical success. A protocol-violating child can
close the channel without exec; a concurrent exit after the last observation can
also occur. An already-observed nonzero/signal exit, partial error record, reported
setup error, read/poll failure or timeout is held. Fast ordinary successful exits
remain compatible.

Failure cleanup applies only to the caller's freshly forked direct child while
it remains unreaped. `waitpid` confirms that direct-child relationship; non-child
or already-unowned PIDs are refused. The single-threaded runner has no competing
SIGCHLD reaper in this lane. No persisted job PID is used for signaling. If the
child is still owned at failure, cleanup sends SIGKILL to that direct child and
polls reaping for up to one second. This is not group/descendant cleanup, a hard
kernel deadline or a solution for another component reaping concurrently.

Failed submission retains its job request, shared envelope, log allocations,
actual child PID and startup diagnostics. A confirmed reaped startup failure is
recorded as failed with stage `startup_failed`. Unconfirmed cleanup is held as
`stalled`/`startup_held`, with no claimed finish time or exit result. No implicit
retry or reuse of the allocated job slot occurs. Metadata publication itself can
still fail; pre-existing state and stages are then retained for reconciliation.

Validation: eight compiled/actual-runner checks cover normal exec observation,
fast successful exit, exec/session-setup failure phase reporting, bounded timeout,
partial records, observed nonzero exit, refusal of non-child signaling and real
submission against a non-executable worker fixture. That actual failure retained
its PID/phase/reaping diagnostics and empty log files without allocating simulation
output. The full affected suite passed 106 checks. The rebuilt source binaries
passed all six supported headless/job-runner integration fixtures.

This extends the app-local storage/execution adapter; shared module APIs, versions
and dependency adoption remain unchanged. Full worker readiness/process identity,
log byte limits, descendant/lifetime supervision, multi-file recovery and forced-
death reconciliation remain open. Source qualification does not establish CFD
accuracy, installed-product freshness or release acceptance.

```sh
make BUILD_DIR=build/<fresh-profile> test-native-job-startup
```

Evidence: `data/experiments/lifecycle-validation/20261007-native-job-startup-observation`.
This newer packet is outside the prepared independent archive snapshot.
