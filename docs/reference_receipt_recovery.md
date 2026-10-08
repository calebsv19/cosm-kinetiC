# Reference receipt recovery operator contract

The standalone recovery tool defaults to a read-only plan. It admits an exact
retained `.receipt-attempts/<uuid>/` and a separately supplied expected context:
`command` (array), `source_sha256` (filename-to-digest mapping), and `artifact_paths`
(array of absolute in-run paths or run-relative paths). Supply expectations from
the known run contract; do not derive them solely from an untrusted candidate.
The run directory must match the source-map digest partition, and frozen source
bytes, publication request, candidate bytes and declared artifacts must match.

The planner opens only existing admitted kernel-lock files and acquires temporary
cooperative exclusion without creating lock files, directories, logs or records.
Missing locks, active owners and unverified candidate terminal scope hold. Limited
terminal evidence requires direct-child reaping, anchored group cleanup and
verified limited terminal scope; complete descendant termination remains false.
These are integrity/cooperative ownership checks, not cryptographic authentication.

Plan command (paths must be absolute):

```sh
python3 -B scripts/reference_receipt_recovery.py \
  --repo /absolute/checkout \
  --data /absolute/checkout/build/c3d-family/runs \
  --receipt /absolute/run/NAME-receipt.json \
  --attempt /absolute/run/.receipt-attempts/UUID \
  --context /absolute/known-recovery-context.json
```

An eligible plan reports `ready_to_publish` and an exact `sha256`. Only an explicit
`--apply --expected-sha256 <planned digest>` can publish. Apply reacquires verified
reference ownership, repeats source/request/artifact/terminal checks, syncs the
candidate, rechecks bytes and atomically links without replacement. Exact final
readback is required. Matching already-published bytes are idempotent readback;
different predecessors, races and changed planned digests hold without replacement.
Candidate/request bytes remain retained. A failed numerical outcome remains failed.

Failure after linking may leave complete final bytes with unconfirmed application;
the CLI reports held/unconfirmed rather than rolling back. Inspect actual bytes
again. This restores a complete receipt candidate, not an interrupted solver,
missing output, incomplete compiler prefix or coherent multi-file operational job.
No automatic retry, deletion, pruning or false success outcome is introduced.

Nine recovery methods and publication/ownership/Make borrowing/record regressions
passed: forty distinct methods total. The existing 194-wrapper/frozen supervisor
bytes are unchanged from the prior full-family publication proof, confirmed by
source digests. The new operator adapter does not rerun numerical campaigns.
Canonical adoption, full operational recovery, authenticated worker lifetime,
archive-backed retirement and guarded pruning remain incomplete.

Evidence packet: `data/experiments/lifecycle-validation/20261007-reference-receipt-recovery`.
