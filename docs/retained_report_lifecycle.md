# Retained convergence report lifecycle

`make test-open-atmosphere-convergence` builds the selected worker and writes a
fresh UUID capsule under `EXPERIMENT_DIR/report-proofs`. It no longer selects
`build/open-atmosphere/convergence/metrics.json`. Earlier fixed reports are
untouched. Each capsule retains request and terminal receipts, metrics when
produced, bounded stdout/stderr, declared Python input snapshots, interpreter
and worker hashes, and a worker copy. Sealed readback detects file corruption
and works after relocation; it is not independent backup or runtime recovery.

The report supervisor holds worker execution ownership through snapshot, run,
input revalidation and sealing. Make ownership is borrowed with kernel-verified
descriptors and passed to the child so nested worker calls do not reacquire it.
A standalone report supervisor acquires its selected worker subtree. The child
has a 120-second wall limit and separate one-MiB stdout/stderr capture limits.
These are trusted local sources, without a disk quota or arbitrary-code sandbox.
Declared input, interpreter or worker drift fails the proof and retains the
partial attempt. If owned-process teardown is unverified, the receipt records
a hold and the attempt remains unsealed. Ordinary reruns never delete or replace an earlier capsule.
Forced supervisor termination can leave an unsealed attempt, which remains held.

The legacy direct test script still accepts `--report` for a fresh retained
location. It refuses existing files and protected build/test/source namespaces
before running tests and publishes using exclusive file creation. Use the Make
proof route to retain full supervision metadata and failure diagnostics. Its
three analytic qualification assertions and metrics remain unchanged; this does
not qualify general plume convergence or physical cube force accuracy.

Complete transitive dependency/environment reproduction, archive coverage,
retirement and migration of other fixed numerical reports remain open.
