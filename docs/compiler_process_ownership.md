# Compiler supervision and staging ownership

The atomic compiler-output wrapper now retains a live direct Python anchor for
the compiler process group. The parent observes exit through a bounded result
pipe and waitid WNOWAIT, signals the group before reaping the anchor, and refuses
signals after loss of the direct-child identity. The actual compiler inherits
the admitted build ownership descriptors; the anchor keeps them until cleanup.
The adapter remains self-contained so frozen minimal source-checkout fixtures
need no new imports or payload dependencies.

The parent-only lifeline is excluded from compiler inheritance. Parent death
causes anchor self-cleanup of its current process group. SIGINT/TERM are forwarded
while the anchor remains alive. Interruption gets a one-second cooperative TERM
window followed by anchored SIGKILL and a five-second direct-child reap window.
The default compiler observation cap is 900 seconds, with a bounded explicit
supervisor API parameter for tests/callers. Bounds are cooperative between calls,
not hard deadlines for process creation, filesystem operations or kernel stalls.
Compiler diagnostics continue through the inherited stdout/stderr streams; this
slice does not impose log quotas on ordinary Make output.

Publication still requires an observed successful command, admitted staged
output/dependency files, unchanged predecessors and ownership receipts. Cleanup
must be observed before publishing. If result/cleanup is uncertain, the stage
is held rather than automatically removed; guarded clean rejects its unknown
bytes. Normal observed compiler failures and confirmed interrupts preserve the
previous final output and remove their disposable stage. Forced wrapper death
preserves the stage and never publishes the partial output.

Verification covers 29 affected checks: 12 atomic-output, five build-owner,
one build-identity, one output-isolation, five configuration-admission,
one contract-writer and four CFD-evidence lifecycle checks. Added behaviors
include a parent poll prohibition, refused cleanup preserving the candidate and
predecessor, bounded TERM-ignoring interruption, and parent-death writer shutdown.
The forced-death ownership fixture now explicitly escapes the compiler into
another session, verifies inherited kernel ownership still excludes overlapping
build roots, waits on actual ownership release, and verifies unknown staged
output remains held. Two intermediate fixture failures are retained with the
final passing logs; they exposed old parent-topology and error-string exit
assumptions, not permission to weaken the ownership invariant.

This verifies the observed compiler/direct-anchor scope. It does not prove
complete descendant termination or retirement eligibility. Escaped descendants,
remaining outer Make/numerical supervisors, aggregate quotas, full transitive
build provenance and dependency/output multi-file recovery remain open.
No canonical adoption, commit, package installation or release is included.
The frozen later backup excludes this new evidence packet.

Shared reuse: core_jobs owns a main-thread function queue and core_workers a
thread pool. Neither is a source-checkout Python process/FD supervisor. Reuse is
deferred for this app-local compiler adapter; no shared module/version changes
are made. Consolidating the repeated source-control adapters requires a separate
provenance and frozen-fixture compatibility review.
