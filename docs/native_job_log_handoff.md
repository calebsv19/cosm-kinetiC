# Native detached job log handoff

Submission now allocates stdout/stderr log descriptors in the guarded parent
before fork. The detached child no longer opens named log paths with append-mode
stdio. Both fixed slots are preflighted as absent; each is then exclusively
created with no-follow, nonblocking, append and close-on-exec flags and private
permissions. A pre-existing regular, linked, special or hardlinked slot is held.
No predecessor is truncated or automatically adopted.

The job root descriptor and both file descriptors are checked against admitted
path identities. The files must be singly-linked regular files and empty at
handoff. Fresh log files and their directory are synced before fork. Allocation
or fork failure closes descriptors and retains any created files for diagnosis.
The parent releases its copies after fork. A later namespace change holds the
child handoff instead of redirecting output into a replacement.

Descriptors are moved above standard-stream numbers before redirecting them.
This also handles a caller with closed stdin/stdout/stderr. The child duplicates
only the selected log descriptors onto stdout/stderr and gives stdin `/dev/null`;
extra descriptors are closed, while standard output descriptors survive exec.
The operation guard is close-on-exec and remains separate from worker lifetime.

This is startup admission, not bounded log capture or whole-lifetime ownership.
After handoff the worker writes through the pinned descriptors; no ongoing log
namespace witness, byte quota, authenticated process identity or bounded application-ready
acknowledgment is introduced. Bounded exec/error channel observation is now
covered in `native_job_startup_observation.md`. Replacement/unlink after handoff, full supervisor
reaping and forced-death reconciliation need later work.
Uncooperative filesystem races and hard syscall deadlines are not excluded.

Validation: nine compiled behavior checks cover exact stdout/stderr and null stdin
through exec, closed standard streams, existing second-slot admission before any
new log, linked/FIFO/hardlinked slots, root/log replacement, in-place modification,
protected/linked roots and descriptor release without evidence removal. The final
affected suite passed 98 checks. Both rebuilt source binaries passed all six
supported headless/job-runner integration fixtures. No solver or installed-product
qualification follows.

This extends the existing app-local guarded storage adapter. Shared module APIs,
versions and dependency adoption are unchanged. Canonical, archive payloads and
public release state are unchanged; no cleanup or pruning was applied.

```sh
make test-native-job-log-handoff
```

Evidence: `data/experiments/lifecycle-validation/20261007-native-job-log-handoff`.
This newer packet is outside the prepared independent backup.
