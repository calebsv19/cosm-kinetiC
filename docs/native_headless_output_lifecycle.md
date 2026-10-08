# Native headless output preservation

Main Edit replaces recursive `--overwrite` deletion with completed-run retention.
Fresh/empty roots receive an exclusive native owner marker bound to directory
identity. Existing nonempty roots require a completed matching marker; unknown,
legacy, running and changed-identity roots are held. The completed predecessor
moves intact into a fresh sibling `<output>.retained-XXXXXX/previous` slot before
replacement allocation. There is no recursive deletion or automatic pruning.
If later work fails, both the predecessor and new attempt remain available.

Root admission rejects linked components except observed standard macOS aliases,
upward traversal, protected storage/app bundles, broad home/system roots, checkout
source storage and runtime-scene input overlap. In a Git checkout, only strict
children of declared generated namespaces are admitted. Directory descriptors
bind owner publication to the admitted root. File/namespace flushes cover owner
records, retained predecessor and fresh-root parent. Final synchronous completion
records a completed marker; early failure or forced termination can leave a
running marker, which remains held. A completed marker allows preserving a run;
it does not certify numerical success or grant deletion authority.

These are cooperative local lifecycle controls. Marker identity is not a
cryptographic payload manifest, process authentication or public sandboxing.
Uncooperative component races, whole-tree integrity, individual sidecar paths,
concurrent external writers and forced-death reconciliation require separate
work. Explicit summary/progress paths and native job-runner metadata remain
separate write-admission gaps. Sibling retention is local preservation, not an
independent backup. Exact archive coverage and retirement remain required.

Thirty compiled-helper/clean/retention checks passed. The integrated headless
build and CLI, Water and scene-project cache-output proofs passed. Actual CLI
readback matched the preserved original two-frame summary and replacement
one-frame summary; the predecessor marker still matches its original directory
identity. No original historical evidence or old build profile was removed.
An older isolated build was correctly held for unowned configuration metadata;
a fresh profile was used. No canonical adoption, package or release occurred.

Evidence: `data/experiments/lifecycle-validation/20261007-native-headless-output-retention`.
This packet and newer output attempts are outside the prepared independent backup.


The subsequent native-sidecar slice closes explicit summary/progress path
admission and replacement holds. Mutable atomic publication and detached-runner
metadata remain open; see `native_headless_sidecar_lifecycle.md`.
