# Shared report preflight before job status publication

Actual-runner probes showed partial status replacement when an empty required
created timestamp or stage made the shared report invalid. Refresh returned an
error after status mutation; the predecessor report stayed unchanged. Checking
that representation only in the second writer was too late.

The shared report is now built once in caller-owned storage, with all scalar and
artifact type/path copy results checked, and admitted by the existing
core_headless_job_report_validate before status publication starts. The admitted
object and artifact array remain alive through subsequent publication; artifact
selection is not regenerated after the first status write. The standalone shared
report writer reuses the same builder and existing publisher. No defaults are
invented to fill invalid timestamps or stages, and no truncation is accepted.

This is a required preflight slice toward coordinated status/report publication.
It prevents known schema/conversion failures from updating only status. It does
not make both files atomic: report staging/flush/rename/namespace failures after
the status publication may still leave one updated. Complete staging, whole-plan
witnesses, retained predecessors, pending holds and explicit recovery remain
required for the two-file transaction. Independent readers may still observe
mixed generations. This repair must not be substituted for that remaining work.

Verification: 25 actual-runner field methods, 10 path methods and 9 operation-guard
methods pass (44 distinct). Ordinary, policy and bundle integrations pass. The
new method tests both invalid required fields through status and cancellation,
verifying exact predecessor status/report bytes and no cancel flag. The prior
native probe observed status mutation in both cases despite an error return.
Existing valid completion recovery and operation exclusions remain covered.
Artifact overflow rejection is source-checked here; the new actual-runner fault
method specifically exercises required timestamp/stage rejection, not every
possible artifact string length.

Reuse-adopted: the already linked core_headless_job validator owns shared report
meaning; the application now invokes it before its purpose-specific publication.
No shared implementation/API/version or minimum dependency changed. The existing
app single-file retained publisher still writes both files. No source format,
solver, commit, installed package, public release or canonical adoption changed.
Authentication, worker lifetime/log bounds, hard workflow quotas, recovery,
archive-backed retirement and the broad lifecycle goal remain incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-job-report-preflight.
This new packet is outside independently archived batches.
