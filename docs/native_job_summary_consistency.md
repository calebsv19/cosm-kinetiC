# Native job summary observation and terminal consistency

Each refresh consumes at most one admitted summary JSON observation, reusing that
same decoded outcome for recovery and diagnostics. Provided summary schema,
artifact class and output root must match the producer/job identity. Requested
frames and steps per frame cannot change. Known counters and result codes are
strict bounded integers. Completed frames cannot exceed requested frames; success
requires all requested frames completed. A provided result code agrees with the
passed/canceled/failed category (0/2/other nonzero, respectively).

A terminal record and consumed summary must agree on outcome. A known terminal
exit-code category must also agree. Persisted terminal jobs cannot regress or
switch outcomes through progress. Contradictions hold status/cancel without
metadata publication or cancellation flags. Refresh works on a temporary record
and copies it back only after admission and successful persistence. Per-file
publication still does not provide atomicity across status and report files;
failed later publication can leave an earlier published file requiring recovery.

A dead/nonterminal job can recover completion from an admitted success summary
with explicit matching completed counters, resetting active step counters to
zero. This avoids producing completed state with unfinished counters when final
progress is absent. Failure/cancel summaries retain current progress counts:
the producer can emit zero terminal summary counts after partial work, so these
are not treated as monotonic progress evidence. Empty active-worker summary
reservations remain outside consumption until the existing terminal/dead gate.
Saved PID liveness remains unauthenticated and is not strengthened by this slice.

Four new behavior cases cover conflicting terminal outcomes, malformed declared
identity/work/result fields, terminal progress regression and successful summary
recovery without final progress. Twenty-two focused checks, 82 affected native
job checks and all six supported source integration fixtures passed. The recovery
fixture includes valid lifecycle timestamps required by terminal report writing.
Earlier failed focused diagnostics are retained.

Optional absent fields remain compatible; full required schemas, summary source
or request authentication, complete transition history, worker lifetime/log
limits, timestamp ordering and recovery across multiple files remain open. This
is Main Edit source validation, not numerical/installed acceptance. No canonical
adoption, commit, pruning or archive transfer occurred. This packet is outside
both existing independent backup and pending frozen seventy-packet snapshot.

Evidence: data/experiments/lifecycle-validation/20261007-native-job-summary-consistency.
