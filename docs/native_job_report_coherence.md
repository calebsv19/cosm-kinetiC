# Existing job report semantic admission

Main Edit status and cancel now bind an existing report to the loaded saved
status before progress/summary merge or cancellation publication. This reader
check is distinct from the generic predecessor preflight used by writers: a
new status transition must remain free to replace the admitted old report.

Existing reports require exactly the current eleven root fields, exact shared
schema family/variant, bounded JSON strings without decoded NUL, and matching
job identity, program, mapped state, stage and all four saved timestamps. The
existing shared report validator is reused after parsing. The artifact array
admits three to five exact type/path objects: required unique summary/stdout/
stderr roles bind to the record, while optional unique volume/render roles bind
to the fixed derived output paths. Unknown roles, missing required roles,
duplicates, extra fields and path/type contradictions hold before mutation.
Artifact order is immaterial. Optional roles need not reflect filesystem growth
since the last publication and do not require directories to exist when read.
The existing writer may publish updated optional roles on a later state refresh.

Missing legacy reports remain permitted explicitly. This slice does not make
legacy records complete paired generations or certify listed artifacts. Direct
readers still observe separate target promotions; no authenticated producer
identity, full worker lifetime ownership, cancellation transaction or installed /
canonical adoption is claimed. The cooperative job guard and strict bounded
regular JSON reader remain the owning admission mechanisms.

Before-code actual-runner counterproof failed in 21 scalar/schema/artifact
subcases across two test methods: syntactically valid wrong reports were accepted.
A decoded-NUL identity case was added to the repaired reader coverage. Matching
reports accept reordering, optional output growth, and ordinary state transitions.
The report-lock test now starts with a genuinely published pair, then forces a
progress refresh under report-parent lock contention; both predecessor byte
strings survive status and cancel failure, and status succeeds after release.
An older progress-boundary fixture now resets its owned report between independent
status scenarios instead of rewriting status under a previous scenario's report.

Verification: 31 field methods, 17 metadata methods, seven native pair methods,
14 recovery methods, ten forward methods, ten path methods and nine operation
methods pass (98 distinct), plus ordinary, policy and bundle integrations. The
initial repaired run retained one stale-fixture failure, corrected as above;
all final gates pass. Logs and exact source/test snapshots are sealed in
`data/experiments/lifecycle-validation/20261007-job-report-coherence`.

Reuse-adopted: existing bounded strict JSON admission, shared report schema /
artifact structures and validator, app-owned state mapping, path derivation,
string copy and guarded refresh entrypoints. No shared source, API, version,
minimum dependency or vendor pin changed. No commit, canonical adoption,
installation, release, pruning or new independent backup occurred. Broad
lifecycle requirements, reader generations, legacy report disposition, worker
ownership/resources, storage retirement and canonical adoption remain incomplete.
