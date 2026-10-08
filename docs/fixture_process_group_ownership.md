# Fixture process-group identity through cleanup

Fixture supervision previously reaped its direct child with poll() before later
signaling the saved group number. A reaped PID/group number can be reused. The
supervisor now observes its direct child using waitid(WNOWAIT), keeping that
identity unreaped through group cleanup. It refuses signaling after a lost/reaped
anchor or changed live group. This requires one direct-child waiter; competing
external waiters are not supported. Non-reaping waitid support is required before
fixture allocation. No persisted PID/group number establishes signal authority.

A live Python anchor is the direct session/group leader. It executes the fixture
shell in that group and reports its exit code over a bounded pipe, then remains
alive until parent cleanup. The parent admits at most 16 result bytes and a single
bounded signed exit-code line. Malformed, oversized, missing or premature-exit
results remain failed/held. The live anchor avoids macOS refusal when signaling a
group containing only a zombie. Group cleanup is requested before reaping the
anchor, and final reaping uses a direct-child witness rather than a racy post-kill
live group lookup. macOS can stop exposing getpgid before exit becomes waitable.

A parent-only lifeline pipe is excluded from the fixture's inherited descriptors.
Loss of that pipe asks the anchor to kill its own current process group, without
using an archived PID. It monitors the pipe while the fixture is running and
while awaiting parent cleanup. Tests observe background work being stopped on
parent-lifeline loss. Forced-death/permission/kernel-stall behavior and escaped
sessions remain incomplete; this is not a universal descendant-supervision proof.

New receipts separate direct_child_reaped, group cleanup requested, anchored
identity and an already-absent group from complete process-group reaping. The old
owned_process_group_reaped field now remains false; killing a group does not
prove that every descendant was reaped. all_external_descendants_verified_terminal,
payload-integrity and pruning authority also remain false. Historical receipts
are unchanged. Refused/unconfirmed teardown stays failed and retained.

Ten fixture-supervision tests plus affected allocation/retention/restore checks
passed (33 total). They include signal-before-reap ordering with poll prohibited,
lost/changed anchors without signaling, parent-loss cleanup, protocol holds,
success/failure reruns, signal interruption, same-group background work and
controlled termination refusal. All six supported integration fixtures passed;
each terminal record bound its session receipt, confirmed direct-child reaping
and anchored cleanup, and retained the complete-descendant hold.

Other inspected helpers (tool_probe.py and agent_session/owned_command.py) have
similar reap-then-signal ordering and remain explicit migration findings. This
slice does not qualify them, authorize retirement, adopt canonical, commit,
transfer archives or qualify numerical/installed behavior. Its packet is outside
the earlier independent backup and pending frozen seventy-packet snapshot.

Evidence: data/experiments/lifecycle-validation/20261007-fixture-group-anchor.

Tool/worker capture has subsequently migrated to live anchored cleanup; see
`tool_capture_process_ownership.md`. Retained file-backed command supervision
remains open. Historical validation packets remain unchanged.
