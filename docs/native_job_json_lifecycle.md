# Native job metadata admission

The native detached runner now reads request, bundle, status, progress and summary
JSON through `physics_sim_job_json_read`. All six owning read sites use this
reader. Status display also validates the object before printing it with the
established spacing and unescaped path format.

Admission is read-only. The reader refuses linked components (apart from observed
standard macOS aliases), protected paths, special files and hardlinks. It opens
with no-follow, nonblocking and close-on-exec flags, admits a nonempty regular
file up to 16 MiB before allocation, and compares descriptor/path identities and
nanosecond timestamps before and after an exact read. A changed observation is
held rather than accepted.

The JSON grammar requires an object root, valid UTF-8 and complete consumption.
Duplicate decoded keys, embedded NUL, nonfinite numbers, comments, trailing data
and invalid syntax are held. Structural limits are 64 levels, 100,000 values,
4 KiB encoded key tokens, 1 MiB encoded string tokens and 128-byte scalar tokens.
These are cooperative input limits, not a complete field schema, authenticated
job identity, hard process memory limit or filesystem-call deadline.

Metadata writes now use the per-file staged publication in
`native_job_metadata_publication.md`. Public runner read/update coordination is now guarded as recorded in
`native_job_operation_guard.md`. Authenticated process ownership, forced-death recovery and consistency across
files remain open. External edits
after observation can still occur. This change performs no migration, deletion,
worker restart, canonical adoption or release.

Validation: nine compiled-reader tests cover grammar, bounded allocation, special
paths, deterministic same-size in-place drift and 40 seeded valid round trips.
Ten actual-runner path tests include duplicate/oversized status and ambiguous or
linked requests. The final affected suite passed 67 checks; all six supported
headless/job-runner integration fixtures passed after rebuilding both binaries.
An initial status-format regression was repaired; its failed log is retained.
The integrated Make path target ran its prior seven tests; the final combined
suite includes all ten. No CFD accuracy or installed-product claim follows.

```sh
make test-native-job-json
make BUILD_DIR=build/<fresh-profile> test-native-job-paths
```

Evidence: `data/experiments/lifecycle-validation/20261007-native-job-json-admission`.
This packet is outside the prepared independent backup.

Known integer/boolean/string admission and bounded live progress/summary
re-observation are now covered in `native_job_field_admission.md`. Full required
fields, cross-field consistency, timestamps and provenance remain open.
