# Integration fixture lifetime and terminal evidence

All twenty fixture entrypoints that allocate through `fixture_support.sh` now
enter `fixture_session.py` before their work. The supervisor runs the existing
script through a live direct-child group anchor under a unique invocation directory in
`tmp/fixture-sessions/<id>/`. It holds an exclusive kernel lock for the invocation,
passes its descriptor to the shell, and preserves cooperative build ownership
and available Make jobserver descriptors. A high-numbered descriptor avoids shell
command-substitution pipe reuse.

Allocation preserves the original `fixture_owner.json` and binds it to the
active session through a registered owner checksum. A separate
`fixture_terminal.json` records the fixture outcome, owner checksum, session
receipt checksum and owned-group cleanup result. It never rewrites the allocation
record or deletes output. Success, nonzero exits and catchable interruption retain
terminal evidence. The default invocation wall cap is one hour; explicit CLI
bounds can be selected up to one day for long trusted-local fixtures.

```sh
make test-fixture-session
make test-fixture-root
make test-physics-sim-headless-water-mode
make test-physics-sim-job-runner-bundle-smoke
```

The context check requires the actual inherited descriptor to match the locked
invocation file, not merely a PID or running JSON state. Descriptor loss or stale
metadata cannot forge an active session. Direct allocation without a valid session
remains supported but is recorded as `allocated_unsupervised`; it does not gain
terminal-retirement evidence retroactively.

If process-group termination is refused, the failed terminal record preserves
that error and does not claim the group was reaped. If a child cannot be reaped or
supervision is forcibly killed, complete terminal recording may be absent; the
allocation stays held. Same-group background children are terminated at normal
completion. Commands that start new sessions or otherwise escape the group are
not thereby proven terminal. Script bytes are bound before and after invocation;
full transitive helper/tool provenance remains separate work.

Terminal records explicitly keep payload integrity, all external descendant
termination and pruning authorization false. Exact inventory, cooperative owner
reconciliation, archive coverage where required and a separate retirement plan
remain necessary. This is lifecycle evidence, not permission to remove fixture
scratch or retained visual output. Older allocation records remain unchanged and
unqualified for terminal retirement.

Six session tests cover success/failure reruns, active kernel ownership, signals,
stale descriptor rejection, background-child cleanup, termination refusal and
entrypoint coverage. The signal suite passed three consecutive runs after the
macOS refusal edge case was addressed. Three allocation tests and six retention
audit tests also passed. Real Water, scene-project cache-output and detached
job-runner bundle fixtures passed and produced bound terminal records. Other
heavy/private fixtures have source integration but were not all rerun here.

## Current cleanup identity and receipt correction

See [fixture_process_group_ownership.md](fixture_process_group_ownership.md).
The current implementation keeps an unreaped live group anchor through cleanup
and separates direct-child reaping from group termination requests. Killing a
group does not prove complete group reaping; new owned_process_group_reaped
values remain false. Historical receipts and the earlier validation narrative
above are unchanged. Terminal retirement eligibility remains incomplete.
