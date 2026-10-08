# Tool and worker capture process ownership

Bounded tool_probe.capture no longer waits/reaps its direct child and then signals
its saved group number. It launches a live direct-child session/group anchor,
which runs the requested command in that group, reaps that command and reports
its result through a separate bounded pipe. The anchor remains alive until
parent cleanup. Parent group signals require an unreaped direct-child waitid
witness and a matching live group. Lost/reaped ownership holds signaling. This
requires one direct-child waiter and non-reaping waitid support; external reapers
are unsupported. No archived PID/group number establishes signal authority.

A parent-only lifeline is excluded from command inheritance. The anchor observes
lifeline loss during command execution and while awaiting cleanup, requesting
SIGKILL against its own current group. Captured stdout/stderr remain independent
bounded byte streams. The anchor closes its own stdout/stderr after the command
result, permitting EOF while retaining live cleanup identity. Descendants with
inherited streams remain subject to the existing timeout. Descriptor inheritance,
combined/independent output caps and bounded cooperative timeout behavior remain.
The internal result is at most 16 bytes and one bounded signed exit-code line;
malformed/oversized/missing outcomes hold. Command launch errno is an unverified
result, not an ordinary successful/failed command exit.

Parent cleanup signals the anchored group before direct-child reap. A refused or
unconfirmed teardown returns unverified with retained captured diagnostics. The
parent closes its lifeline and uses a direct-child witness before final wait.
The existing terminal_processes_verified compatibility flag now explicitly
covers observed command result plus reaped direct anchor and confirmed cleanup
request admission. It does not claim complete descendant termination. Outputs
include terminal_verification_scope, direct_child_reaped,
group_cleanup_identity_anchored, command_result_observed and
all_external_descendants_verified_terminal=false. Timeout/output failure without
an observed command result cannot gain terminal verification from cleanup alone.
Retained report and atmosphere receipts propagate this scope and the descendant
hold; integrity sealing is not pruning or lifetime authorization.

Four new behavior cases exercise live signal-before-reap with direct poll
prohibited, lost anchors without signaling, parent-lifeline loss, malformed result
messages and launch failure. Fifteen focused capture checks and 49 affected
build/setup/doctor/report/atmosphere checks passed; all six supported source
integration fixtures passed. Source-copy fixtures still need only tool_probe.py;
the internal anchor lives in that same file and adds no fixture-session import.
This app-owned Python adapter remains local; no shared native API/version or
canonical adoption changed.

Full escaped-descendant ownership, aggregate/hard kernel deadlines, complete
helper/interpreter provenance and multi-file recovery remain unqualified.
agent_session/owned_command.py still has similar ordering and requires migration.
No commit, pruning, package/install/release or archive transfer occurred. The new
packet is outside the earlier independent backup and pending frozen seventy-packet
snapshot, which remains unchanged.

Evidence: data/experiments/lifecycle-validation/20261007-tool-capture-group-anchor.

The file-backed retained-command supervisor subsequently migrated to live anchored
cleanup with nested TERM grace. See `retained_command_process_ownership.md`. Full
process ownership/descendant audit remains incomplete; prior packets are unchanged.
