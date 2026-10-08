# Whole-Make process ownership

The outer build owner now retains a live unreaped Python process-group anchor
through command observation and cleanup. SIGINT/TERM forwarding and final group
shutdown use waitid WNOWAIT and live group identity before signaling. Reaped or
lost anchors never authorize signals using their saved group number. The actual
Make command can exit while the anchor remains alive for group cleanup.

Build hierarchy descriptors and admitted GNU Make jobserver descriptors are
inherited through both anchor and command. Existing ancestor/root exclusion,
sibling independence, command-line overrides, read-only Make routes and mixed
clean/build refusal remain intact. Descriptors close only after supervision;
escaped descendants keep their inherited ownership and continue excluding
conflicting builds/cleanup. They are not declared terminal by group signaling.

Parent-only lifeline EOF sends TERM to the anchor's own current group, allows a
five-second cooperative nested cleanup window, then sends KILL. The parent has
the same TERM window and a five-second direct-anchor reap bound. Ordinary Make
observations default to 86,400 seconds; the supervisor API admits only finite
positive bounds through that ceiling. These bounds are cooperative between
observations, not hard deadlines for process creation, filesystem/kernel stalls
or an aggregate workflow budget. Ordinary Make output remains inherited and has
no new log quota in this slice.

A bounded result pipe reports direct-command completion. Unobserved results,
refused cleanup and uncertain direct-anchor reaping cause an ownership hold.
The owner does not prune or register unknown output on failure. Compiler staging
holds remain governed by the atomic-output adapter. Nested cleanup gets time to
publish its own observed failure state before outer group force termination.

Thirty-one affected checks passed: twelve Make owner, twelve atomic compiler,
five configuration admission, one build identity and one output isolation.
Actual recursive parallel Make passed with no jobserver-unavailable warning.
Tests cover parent poll prohibition, nested TERM cleanup completion, parent death,
lingering same-group writers after command exit, reaped saved-group refusal,
bounded TERM-ignoring shutdown and unknown partial-output preservation. The
escaped-compiler fixture still proves inherited kernel exclusion and waits on
actual ownership release before assessing held staged bytes.

The verified scope is observed direct command plus reaped direct anchor and
anchored group cleanup. Complete descendants, forced kernel-death recovery,
remaining older numerical-run supervisors, retirement/pruning and canonical
adoption remain incomplete. No commit, installation, release or archive upload
is included. This evidence is outside the frozen prepared backup cutoff.

Shared reuse remains deferred: core_jobs and core_workers describe in-process
queues and thread pools rather than checkout Python process/FD supervision.
This local adapter preserves existing standalone fixture copies without adding
new imports. No shared module/version or adoption state changed. Unifying the
control adapters requires separate provenance and frozen-source compatibility
review rather than silently widening these source-checkout contracts.
