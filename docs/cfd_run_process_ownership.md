# CFD retained-run process ownership and resource observation

The standard-library CFD run helper now retains a live unreaped direct anchor
through cleanup. The outer parent never polls that anchor before group signaling.
The anchor observes its direct command with waitid WNOWAIT while sampling RSS,
so a completed command's PID cannot be recycled during the memory read. The
anchor is the only command waiter. The parent's result pipe has a 512-byte limit,
exact framing/fields/types and duplicate-field refusal.

RSS reads use fixed /bin/ps with a one-second observation timeout. A missing or
invalid observation while the command is still live holds the run; a command
that has already terminated may legitimately lack a final sample. The observed
peak remains sampled direct-command RSS, not aggregate descendant memory or a
hard memory quota. Brief peaks can occur between samples. On a sampled cap
violation the anchor terminates its still-unreaped direct child, waits for its
result, and reports failure while staying alive for outer group cleanup.

Execute retains its existing command, wall_s, sampled RSS and exit_code fields.
It also reports direct-anchor reaping, anchored cleanup and the limited terminal
verification scope. Complete descendant termination stays false. The helper now
admits finite positive wall bounds through 86,400 seconds, integer RSS bounds
through one TiB, and a sampled combined retained-log cap (64 MiB by default,
optional through one GiB). Log/time bounds are checked between operations and
allow sampling overshoot; process creation, filesystem/kernel stalls and whole
workflow aggregate resources are not hard bounded by this slice.

The parent-only lifeline is excluded from command inheritance. Parent loss
causes current-group self-cleanup. Ordinary interrupt/wall/log failure allows
bounded TERM observation before anchored KILL and direct-anchor reaping.
Missing command outcome or refused cleanup retains diagnostics and returns an
unverified teardown. It cannot publish a retained compiler binary. Compiler
receipts preserve the candidate, failed request/logs, sampled RSS when observed,
and explicit descendant/terminal scope. Existing no-replace publication and
predecessor preservation remain unchanged.

Twenty-five distinct affected checks passed: eight new supervision checks,
five retained compiler checks (including actual Clang), and twelve evidence
admission checks. The actual control-only `make test-cfd-run-supervision` route
also passed all eight checks without normal build fragments. Behaviors cover
success/nonzero logs, measured direct RSS, an 80 MiB allocation exceeding the
32 MiB cap with retained unpublished candidate, wall/log failure, reaped identity
refusal, invalid inputs before launch/log allocation, and parent loss.

This helper is frozen directly into six numerical/readback source families and
adds no new import/payload dependency. Older independently implemented numerical
supervisors still require audit/migration. No numerical campaign was rerun and
these results do not qualify CFD accuracy or complete descendant lifetime.
Retirement, canonical adoption and complete writer coverage remain open.
No commit, installation, release or archive upload occurred. This packet is
outside the frozen later prepared backup.

Shared reuse is deferred: the existing core_jobs/core_workers in-process queues
and thread pools do not implement checkout Python process/RSS/FD supervision.
No shared module, version or adoption state changes. The self-contained control
adapters need a separate consolidation/provenance compatibility review.
