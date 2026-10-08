# Retained package proof capsules

Source/Main Edit package self-tests, Linux worker validation/dry-run, Linux desktop
self-test/determinism and Main Edit refresh process-audit now run through
`scripts/package_proof.py`. Existing prerequisite and authority gates remain in
place. No real package, signer, install or refresh ran for this slice.

Each invocation creates `<selected-package-root>/.package-proofs/<name>-<id>/`.
Its receipt and separate command stdout/stderr logs live outside a fresh work directory. Historical combined logs remain retained. The
receipt binds package input inventories, declared identity values, proof control
source, interpreter/command binary and relevant environment. The command receives
explicit work-relative output mappings; fixed self-test HOME, runtime/state,
process-audit and comparison paths are replaced by capsule paths. Successful and
failed payloads are retained. Neither parent roots nor preceding proofs are reset.
Legacy self-test directories remain untouched and separately held by cleanup.

The wrapper holds checkout cleanup exclusion and, where the package transaction
owner lock exists, shared package ownership against assembly/recovery. Recursive
Make retains build-owner and jobserver descriptors. Concurrent proof invocations
use distinct capsules and can inspect the same unchanged package. Input drift
prevents a passed result. Normal clean holds the package_proof artifact class.
Capsule retention/export/pruning has no automatic grant and needs the existing
archive/readback lifecycle before removal.

The macOS session validator now accepts an explicit fresh --output-root. The
Make proof passes its own session-proof directory. MCP requests, stdout, stderr
and command/timeout state persist; both success and failure remain inspectable.
Standalone invocation also allocates and retains a fresh temporary output root
and prints its location. Reusing an existing explicit root is refused. Session
handshake output and package input unchanged checks are separate evidence.

A passed capsule proves only the exact local command and unchanged recorded
inputs. It is not installed/public acceptance, solver accuracy, authentication,
promotion or release authority. Main Edit refresh still requires its existing
self-test/process guards and uses predecessor preservation; its proof receipt
explicitly leaves installed/published verification false. Canonical's existing
one-worktree refresh gate remains unresolved and unchanged.

Nine proof tests cover retained success/failure, input drift, concurrent CLI
capsules, mapping/source-root refusal, active cleanup/package ownership, an actual
macOS self-test Make recipe with fake launcher/validator, retained session MCP
requests/replies and timeout output, and refusal to overwrite a used validator
root. Twenty transaction, eight admission, nine cleanup and five ownership checks
plus Main Edit source contract pass. The actual macOS fixture stubs package smoke;
Linux and real app/package behavior remain source-inspected and unqualified here.

A bounded scan now finds no rm -rf in make/package*.mk or make/release.mk. It does
not prove arbitrary scripts or release audit/signing paths have complete ownership
or immutable proof lifecycle. Those remain distinct work alongside full artifact
classification, refresh ownership reconciliation and canonical adoption.

## Bounded command execution and terminal evidence

Package proof children now reuse the retained contract command runner. The default
wall limit is 900 seconds, configurable up to 3600 with --wall-cap. Combined
stdout/stderr has a 64-MiB sampled cap, reducible with --log-cap. Output is streamed
to exclusive command.stdout and command.stderr files. A child can exceed the cap
between samples; excess diagnostics stay retained. This is not a hard disk quota,
untrusted-code sandbox or bound on input/output inventory traversal.

Catchable SIGINT/SIGTERM forwards to the owned process group, with final teardown
and a five-second direct-child wait. Known failed exit codes remain in receipts;
timeout/overflow leave the exit status unqualified. Signalling/wait uncertainty
records terminal_processes_verified=false and does not inventory active work.
Verified failures retain available work inventory; unreadable/special work gets
an inventory-error field and a failed terminal receipt. Failure diagnostics name
the retained capsule. Forced supervisor death and escaped descendants remain
outside this qualification; running/uncertain attempts stay held for review.

Fourteen proof tests pass, including a real SIGTERM child/cleanup-lock test,
partial timeout/overflow, teardown uncertainty, special work and existing actual
Make fixture behavior. Seventeen contract regressions pass. Four controlled CLI
runs retain success, exit-code failure, timeout and overflow in
build/lifecycle-package-proof-bounds-20261007a. Exact newly allocated capsule IDs
and copies are sealed in the 20261007-package-proof-command-bounds lifecycle
packet. These are controlled fixtures; no real package, signer, install or release
ran. Later independent archive coverage and canonical adoption remain open.

## Bounded package and Desktop inventories

The reused Desktop byte/mode/link inventory now bounds each tree to 100,000
entries, 32 GiB hashed bytes, 8 GiB per file, depth 256 and 120 seconds. These
limits apply to package proof inputs/work and transaction assembly/reuse/recovery
inventories as well as Desktop bundle validation. Package proof receipts record
the defaults. Limit exhaustion holds the operation; it does not omit entries or
accept a partial identity. Internal bundle links and empty directories preserve
the existing inventory representation.

Enumeration checks limits before accumulating children. Regular files are opened
through no-follow ancestor descriptors, checked against admitted identities, and
hashed through bounded chunks with nonblocking admission to prevent FIFO swaps
from hanging. Directory/file identities are rechecked after traversal. Parent
swaps, content mutation, late directory additions and special files are refused.

These are per-tree cooperative bounds checked between filesystem operations;
blocked syscalls and aggregate multi-tree/workflow time or memory are not hard
bounded. Metadata rechecks detect observed drift, not an atomic filesystem
snapshot. Traversal does not follow symlink leaves. Symlink target admission
retains the prior internal-only contract.

`make test-artifact-inventory` uses the control-only route and passes with absent
compiler/pkg-config selections. Eight inventory checks, fourteen package-proof,
twenty transaction and fourteen Desktop preservation checks passed. Four fresh
controlled package-proof CLI capsules record these limits and retain expected
success/failure states. Evidence is sealed in
`data/experiments/lifecycle-validation/20261007-package-inventory-bounds`.
No real package, signing, install, release, canonical adoption or survivor cleanup
occurred; later independent backup coverage remains open.

## Fresh release bundle audit reports

`release-bundle-audit` preserves its package-desktop-self-test prerequisite and
uses a fresh package-proof capsule under RELEASE_DIR. Bundle identity, launcher
config and binary dependency reports live in its work directory. Complete exact
output admission precedes writes. Framework dependencies use distinct numbered
reports and an index mapping, including spaced paths and duplicate basenames;
regular dylibs are discovered through the bounded inventory. The framework helper
reserves every report path and refuses protected/existing outputs. Each framework tool call
has a 30-second timeout and a 1-MiB accepted report-size ceiling; excess output is
retained as failed diagnostics, not capped while being written. The outer proof
still supplies command lifetime/log bounds. Existing runtime/Vulkan/portable-link
checks remain active. Exact audited-file header lines are excluded from portability
checking; raw headers remain in the report and real dependency lines retain the
same rejection patterns. Legacy release-directory reports remain unchanged.

`release-contract` is now read-only and compiler-independent: it prints expected
source release settings without allocating RELEASE_DIR or entering build ownership.
Credential-profile presence uses Make's empty/nonempty test and does not print
profile values. Contract output does not verify artifact, authentication or
publication state. `make test-release-audit` uses the control-only route.

Seven actual-recipe fixtures, fourteen package-proof and eight output-admission
regressions pass. A retained fixture runs two successful audits plus a rejected
nonportable framework dependency, with legacy reports preserved. Evidence is in
`data/experiments/lifecycle-validation/20261007-release-audit-writers-final`.
Tools/package prerequisites are fake in these fixtures. Real package/platform,
SDK tool provenance, signing/notarization/artifact writer lifetime, canonical
adoption and later independent backup coverage remain open.

## Aggregate package identity passes — October 7, 2026

Package input and output identity passes share an InventoryBudget across selected
roots. Entry counts, hashed bytes and elapsed deadlines cannot reset per tree;
missing input selections also count. Limits remain cooperative per identity pass,
not a hard quota or whole-workflow deadline. Combined output exhaustion retains
the failed attempt and refuses publication. See [current setup review](top_level_setup_review_20261007.md).
