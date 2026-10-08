# Retained local worker authoring and validation attempts

Main Edit scene authoring and validation no longer use temporary directories that
are removed on exit. Each invocation allocates a fresh
`<session-root>/attempts/author-<uuid>` or `validate-<uuid>` identity, with request,
receipt, stage and exclusive stdout/stderr files. Failed attempts are preserved;
retries create new identities rather than resetting previous inputs or logs.

Authoring writes its scene inputs into the retained stage. On success, the stage
is published as the scene, while the attempt keeps its request and command logs.
The scene manifest identifies its author attempt, revision and worker checksum.
New scenes may be reused only when that exact attempt has a completed receipt.
Failed publication, changed worker bytes or uncertain teardown remains held for
reconciliation, including any already-published scene. Legacy scenes retain their
existing admission; this change does not silently rewrite their provenance.

Validation retains its resolved request, runtime scene and copied assets. The
worker's successful exit is insufficient: output must pass bounded strict JSON
readback and contain the expected validation fields. Failed/malformed output and
requests remain available together. Ordinary failure messages identify the
retained attempt. The initial request also survives unavailable worker admission.

Both operations use the same packaged-compatible bounded command supervisor as
retained contract proofs. Contract proof control provenance now includes that
executor's bytes. Each session command has a 30-second wall limit and a sampled
1 MiB combined stdout/stderr limit. Timeout/failure terminates the owned process
group and reaps the child. Cooperative nested supervisors get a bounded grace
period before forced group termination. Teardown errors produce a held receipt
with `terminal_processes_verified=false`, never a completed receipt.

Signal forwarding is installed in the main thread. Threaded invocations retain
wall/log limits and ordinary child teardown without registering process signal
handlers. Forced termination of the service, escaped process groups, hard kernel
resource limits and complete descendant/owner reconciliation are not proved by
this slice. Receipts left running are held; no automatic restart or deletion is
implemented. Sampled log caps may overshoot between checks.

Worker bytes are admitted as a regular file up to 128 MiB and rechecked after the
attempt. This is input drift detection, not a complete executable/library trust
contract or immutable filesystem snapshot. All attempt data remains operational
storage; exact backup coverage and retirement require separate owner action.

The macOS source package's existing `agent_session/*.py` copy includes these
helpers. No package build, installed update, release or canonical adoption occurs
in the source tests. Tests cover failed reruns, retained validation inputs, wall
and log limits, teardown holds, worker drift/unavailability, malformed successful
output, linked attempt storage and threaded execution. Existing session, sample,
path and contract suites validate the affected normal source workflows.

Final checks: ten focused/control-only attempt tests, fifty affected operational
and contract-proof regressions, and sixteen guarded-clean checks passed (76 total).
Operational/session/evidence classes now override even an exact disposable-build
claim, so new attempt records cannot become ordinary clean targets.
Evidence: `data/experiments/lifecycle-validation/20261007-retained-session-attempts`.

## Preserved worker and control inputs

Attempts now copy the admitted worker bytes and flat `agent_session/*.py` control
set into `source/worker` and `source/runtime_modules`. The current on-disk control
set is captured when the attempt module is imported. Changed, added or removed
control files require a new source process before an attempt can proceed. The
set is checked again after the command. Worker executable bits are admitted and
permission changes also hold completion. The preserved worker is an input payload;
the command still invokes its original admitted path to preserve runtime-library
resolution. The snapshot is not automatically executed or restored.

Control capture allows at most 64 Python files, 4 MiB per file, 32 MiB total and
256 directory entries. Sources are regular no-follow inputs. Exclusive copies,
readback and namespace flushes precede effects. Retained payloads and their exact
file set are rechecked before completion. Original-input stability alone cannot
hide a changed retained snapshot. Receipts bind control hashes and worker mode.
A completed receipt requires successful command evidence for the selected worker;
missing evidence stays held with terminal verification false.

This captures on-disk runtime source under cooperative import/update assumptions.
It does not authenticate loaded Python bytecode, the interpreter, native shared
libraries, package signatures or a complete transitive dependency set. The
`complete_dependency_capture` field remains false. Worker-path admission and
readback are cooperative, not an immutable kernel snapshot.

Seventeen focused/control-only checks and thirty-three affected operational
regressions passed (50 total). Evidence:
`data/experiments/lifecycle-validation/20261007-session-attempt-input-capture`.


## Session storage identity witnesses

Each client now pins four non-inherited descriptors for its root, scenes, runs
and service lock. Public operations check those identities before and after
execution; locks compare the acquired file with the pinned lock. Authoring and
validation also check storage before completing retained attempts. Validation
uses the existing service lock. Ordinary directory or regular lock replacement
is held, including replacement during an operation. The original incomplete
receipt remains held rather than writing a terminal receipt into a replacement
root. Constructor admission creates the empty service lock when absent.

Client close releases the four descriptors and prevents reuse; it does not stop
a native worker. These are cooperative checks, not descriptor-relative writes
or protection against transient uncooperative races. Individual scene/run
ownership, native worker storage and forced-death reconciliation remain open.

Twenty path checks, eighteen retained-attempt checks and nineteen operational
regressions passed (57 total). Evidence:
`data/experiments/lifecycle-validation/20261007-session-storage-witnesses`.
This packet remains outside the prepared independent backup and canonical.
