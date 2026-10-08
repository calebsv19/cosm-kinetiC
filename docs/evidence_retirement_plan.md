# Exact evidence retirement coverage plan

The read-only `evidence_retirement_plan.py` supplies the archive-coverage portion
of TL11. Select one sealed bundle under checkout data/experiments and the exact
sealed copy/restore receipt.json files. The selected bundle cannot overlap those
recovery receipts. Preparation receipts cannot establish independent coverage.

```sh
make test-evidence-retirement-plan
python3 -B scripts/evidence_retirement_plan.py \
  --bundle /absolute/checkout/data/experiments/one-sealed-bundle \
  --copy-receipt /absolute/sealed-copy-packet/receipt.json \
  --restore-receipt /absolute/sealed-restore-packet/receipt.json
```

The tool verifies sealed receipts, binds restoration to the exact copy-receipt
hash, archive destination and payload checksum map, and matches the selected
bundle-relative identity/file count/manifest digest against restored coverage.
It verifies the current and restored bundle manifests and compares the complete
regular-file byte/hash inventories and directory shapes. This includes each
bundle_manifest.json and inert service.lock bytes normally excluded by sealing.
Unknown/missing/changed files, extra empty directories, links, hard links, special
files and unmatched later bundles are held. Recovery copies must have distinct
storage paths from the selected source bundle.

The output records exact current/restored file and directory identities, both
receipt hashes, policy version/digest and archive provenance. Every snapshot is
read back before return. The planner creates no files, locks or roots and has no
apply mode. The cooperative per-snapshot preflight caps are 100,000 entries,
32 GiB aggregate regular bytes, 8 GiB per file and 120 seconds between operations.
Hashing/verification calls do not have hard kernel deadlines; repeated snapshots
are not an aggregate workflow bound or protection against hostile external
filesystem races. Trusted local cooperative operation remains the boundary.

A covered snapshot returns covered_snapshot_held, not deletion eligibility.
Terminal ownership, class-owner retirement eligibility and explicit guarded
pruning remain unverified/unauthorized. Local sealed receipts are integrity-bound
historical observations, not cryptographic authentication or a new remote
archive availability check. `remote_archive_rechecked_now` is false. Full
operational-job, fixture, environment, package and authenticated-artifact
retirement routes still require their owning contracts.

Eight new behavior tests plus affected restore/retention checks passed (20 total).
The control-only Make entrypoint passed all eight planner checks without ordinary
build setup. A live read matched 144 current files in lifecycle-validation/
20261007-b2 against the original independent restore receipt chain, including the
manifest. The later native-job-summary-consistency bundle was held as outside
that snapshot. No deletion, canonical adoption, commit or archive transfer was
performed. The pending frozen seventy-packet payload remains unchanged; this
new evidence is outside it.

Evidence: data/experiments/lifecycle-validation/20261007-evidence-retirement-plan.
