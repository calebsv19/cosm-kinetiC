# Retained file-backed command cleanup identity

agent_session/owned_command.py no longer polls/reaps the direct child before
later signaling its saved group number. A live direct-child session/group anchor
runs the command, reports its result through a bounded pipe, and remains alive
until parent cleanup. Parent signaling requires an unreaped direct-child waitid
witness and matching live group. Lost/reaped identity holds rather than signaling
a persisted number. A single direct-child waiter and waitid(WNOWAIT) support are
required. The anchor can reap the actual command because its own live identity
continues to reserve the group number.

The anchor retains caught INT/TERM dispositions while command exec resets those
dispositions. Group termination reaches the command/nested supervisors while the
anchor remains alive. On timeout, log overflow or interruption, the parent gives
nested supervisors up to five cooperative seconds to report their outcome before
requesting SIGKILL against the anchored group. Reaping follows cleanup. A
parent-only lifeline lets the anchor request TERM, wait briefly for its command,
and then kill its own current group if parent connectivity disappears. Tests
observe same-group work stopped after lifeline loss. Escaped sessions, external
waiters and hard kernel/aggregate deadlines remain unqualified.

Existing create-only stdout/stderr logs, inherited descriptors and sampled log
limits remain. Tags now refuse path traversal and invalid characters before log
allocation; invalid/nonfinite time or log bounds also hold before launch. The
result channel admits at most 16 bytes and a bounded signed exit code; missing,
malformed or unobserved outcomes remain unverified. Command launch errno remains
an error. Unconfirmed teardown raises IncompleteTeardown, retaining an unsealed
attempt instead of accepting a result. Logs are preserved on every failure.

Successful execution results and failure objects carry a limited terminal scope.
Session, native-contract, package-proof, assembly and recovery receipts retain
all_external_descendants_verified_terminal=false and describe owned command
outcome/anchor/local-operation scope. The terminal_processes_verified compatibility
flag does not authorize retirement or establish complete descendants terminal.
Per-file receipts and logs do not establish multi-file transaction recovery.

Eight new behavior cases cover success/nonzero logs, live signal-before-reap
ordering with poll prohibited, lost anchors without signaling, nested TERM grace,
parent-lifeline cleanup, refused teardown, malformed/oversized result messages and
invalid prelaunch bounds/tags. Eighty-four affected retained-command, session,
contract and package checks passed. Two isolated actual-Make fixtures omitted the
already-required owned_command.py; their staged source inputs were corrected,
without removing assertions. Initial failed diagnostics remain retained.

A real retained test-cfd-obstacle3d-box invocation compiled and ran successfully
through the final helper. Its fresh 231-file sealed capsule verified and its
compile/run records confirmed direct-anchor reap plus anchored cleanup. This is
native lifecycle proof, not numerical CFD qualification or package/install proof.
The separate native capsule and this validation packet remain retained.

This completes the identified reap-then-signal migration for fixture, bounded
capture and file-backed retained-command supervisors. It is not a complete writer
or process-ownership audit. Worker lifetime authentication, escaped descendants,
resource quotas, forced-death recovery, retirement eligibility, guarded pruning
and canonical adoption remain open. No commit, pruning, archive transfer or
canonical changes occurred. New packets remain outside earlier independent
backup and pending frozen seventy-packet snapshot.

Evidence: data/experiments/lifecycle-validation/20261007-retained-command-group-anchor.
