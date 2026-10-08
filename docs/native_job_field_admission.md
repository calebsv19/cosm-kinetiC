# Native job known-field admission

The runner no longer accepts floating-point values or silent overflow conversion
for its native integer fields. `json_get_int` requires an integer within C int
bounds. Request integer controls are nonnegative and retain their existing
positive/zero constraints after conversion. Provided booleans must be JSON
booleans. Bounded request strings must fit their receiving fields.

Status admits every provided PID/counter as a nonnegative integer within int
bounds; provided exit codes must be -1 through 255. Provided strings must fit
without truncation, so copying cannot turn a longer identity or path into a
matching prefix. Progress counters have the same integer range admission, and
provided stage/time strings fit the receiving record. Unknown progress status
labels and malformed known fields hold refresh before record publication.
When a summary is consumed, only `passed`, `canceled` or `failed` is admitted.

The worker's initial empty progress reservation is distinct from published
metadata while the status is still starting. An active worker's empty summary
reservation is not consumed prematurely. Existing invalid metadata is not treated
as missing or replaced with defaults. Optional absent fields remain compatible;
this is not a complete required-field schema, counter-consistency contract,
timestamp grammar, provenance qualification or process authentication. Unknown
extension fields remain outside this admission table.

Live progress/summary can be atomically replaced while read. The ordinary strict
reader still holds the changed snapshot and reports identity drift as EAGAIN.
Only these mutable observations may retry that read, at most eight attempts and
with a cooperative 100 ms deadline between reads. Syntax/path/field failures do
not trigger retries. This does not retry a mutation, restart a worker, relax
identity checks or impose a hard filesystem/parser-call deadline.

Validation: ten compiled/actual-runner field checks cover exact bounds, fractions,
oversized numbers, null/string/boolean coercions, string truncation, request holds
before slot allocation, status/progress holds before publication/cancellation,
summary labels and startup reservations. Four deterministic observation checks
cover stable readback after one drift, the eight-read cap, malformed input without
retry and the cooperative deadline. The final affected suite passed 120 checks;
both rebuilt binaries passed all six supported source integration fixtures.
An earlier live cancellation fixture hit snapshot drift; bounded re-observation
resolved the ordinary workflow. That same failed fixture was cooperatively
canceled, its canceled summary read back and its diagnostics retained. No worker
restart or saved-PID signal was used.

The tables remain app-owned native policy; no shared module API, version or
adoption changed. Full required-field/schema and cross-field consistency,
authenticated identity, worker/log lifetime bounds, multi-file recovery and the
remaining writer audit stay open. Numerical and installed/public acceptance
remain separate. Canonical and the pending seventy-packet archive payload are
unchanged. This new packet is outside both the original and pending snapshots.

```sh
make BUILD_DIR=build/<fresh-profile> test-native-job-fields
PYTHONPATH=tests python3 -B -m unittest test_job_json_observation
```

Evidence: `data/experiments/lifecycle-validation/20261007-native-job-field-admission`.
