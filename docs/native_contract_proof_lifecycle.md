# Retained native contract proofs

Six box contract targets now allocate a fresh capsule under
`EXPERIMENT_DIR/contract-proofs/<target>-<uuid>/` on every invocation:

- `test-cfd-obstacle3d-box` and its sanitizer target.
- `test-cfd-obstacle3d-box-material` and its sanitizer target.
- `test-cfd-3d-box-session` and its sanitizer target.

Their old selected-build binary and log paths are provenance hints. The recipes
never reset, overwrite or adopt those earlier files. Current stdout, stderr,
binary, source capture and terminal receipt live in the printed retained capsule.
Compile and runtime failures retain their logs and seal regular artifacts for
readback. Forced termination can leave an unsealed running attempt, which requires
preservation and review; there is no automatic deletion or resume.

```sh
make test-contract-proof
make BUILD_DIR=build/my-contract-profile test-cfd-obstacle3d-box
python3 -B scripts/cfd_evidence.py data/experiments/contract-proofs/<printed-capsule>
```

`contract_proof.py` freezes declared source files, public headers, adjacent headers,
and recursively resolved local quoted includes, including included C files. The
compiler uses that captured source tree. Source, compiler, controls and output
identities are checked before terminal success. Macro-generated includes,
external SDK/library dependencies and complete transitive replay are not qualified.
System JSON flags use the selected Make dependency contract.

Direct CLI entry acquires cooperative build ownership. Child compiler and test
processes inherit ownership descriptors, so cleanup cannot enter while they run.
Each command has a 900-second default wall cap. Combined retained logs have a
64-MiB sampled cap; a writer can exceed it between samples, and excess bytes are
retained on failure rather than silently truncated. Completion or failure reaps
the owned process group. Trusted commands can escape that convention; this is not
an untrusted-code sandbox or a hard disk quota.

`retained_contract_proof` is a held artifact class, never disposable build output.
The existing historical contract evidence remains in place. Broad pruning needs
separate archive coverage, readback and retention-policy work.

Lifecycle tests cover distinct reruns, legacy-byte preservation, local C include
capture, compile/runtime failures, source drift, path admission, log/time bounds,
cleanup-class holds and all six actual Make recipe bodies. The initial real
material compile exposed an included-C capture gap; that failed capsule remains
sealed and retained. Corrected real native outcomes are recorded in the current
validation packet. These outcomes are finite contract and sanitizer checks, not
new cube-force accuracy, transient physical acceptance or installed qualification.

## Retained periodic JSONL and assessment

The normal unforced periodic recipe now writes `results.jsonl` inside a fresh
contract-proof capsule and assesses it using a frozen copy of the unchanged
Python assessment. The sanitizer recipe retains `sanitizer.jsonl` with its
existing `small` argument. No fixed build JSONL is selected or replaced. The
JSONL copy is exclusive, hashed and rechecked after assessment; raw stdout and
stderr remain retained. Assessor and interpreter identities join the receipt.
Python assessment ignores environment optimization settings so its existing
assertions cannot be silently disabled. Numerical compiler flags and C assertions
remain unchanged. No broader wall/open/obstacle qualification is implied.

Supervisor teardown has a five-second final wait. Signalling or wait failures
retain an unsealed attempt with terminal-process verification false. A failed
compile/run/assessment with verified teardown is sealed with its diagnostics.
These controls cover cooperative trusted local children; forced supervisor death
and escaped descendants still need separate recovery handling.

## Retained mixed-refinement series

`test-cfd-refined-mixed` allocates fresh native JSON for resolutions 8, 16 and 32.
`test-cfd-refined-mixed-coupling` retains its matrix JSON and builds a fresh native
companion in a nested capsule. The unchanged reference assessment consumes those
current companion files rather than relying on optional shared historical output.
Each command keeps separate stdout/stderr, and a failed resolution stops later
resolutions while preserving earlier results. Partial failures with verified
teardown are sealed; uncertain companion teardown holds the outer attempt too.

The assessment uses its explicitly selected reference venv invocation, preserving
its prefix. Interpreter and optional pyvenv.cfg identity are recorded; the config
is copied into the capsule. Source/control snapshots and assessment data are
rechecked before success, and the nested native bundle is reverified. Corruption
of frozen or companion inputs cannot pass. Full installed-package/SDK identity,
escaped descendants and forced supervisor death remain separately unqualified.

Seventeen lifecycle tests cover series reruns, historical sentinels, partial
failures, invalid arguments, data/frozen/nested drift, venv prefix and config drift,
and teardown holds alongside existing contract behavior. The final two real Make
recipes passed. Evidence is retained in
`data/experiments/lifecycle-validation/20261007-refined-retained-series`.
Numerical gates and solver sources were preserved. This is finite algebra and
lifecycle proof, not broader native force/transient or installed qualification.

## Retained app-stage foundation — October 7, 2026

The [app-stage lifecycle](release_app_stage_lifecycle.md) adds separate signing
and stapling transactions with fake-tool proof, macOS metadata-preserving
staging/publication/recovery, and fail-closed recovery teardown records. Nested
command supervisors receive a bounded cooperative teardown window before forced
termination. Legacy signing/notary/export recipe cutover and real authentication
remain incomplete. No actual signing, notarization, installation or publication
was performed.
