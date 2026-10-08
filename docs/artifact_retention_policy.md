# Artifact retention policy and read-only inventory

`config/artifact_retention_policy.json` is the versioned PhysicsSim pilot policy.
It distinguishes rebuildable compiler products, fixture scratch, retained visual
and numerical evidence, compiler/semantic/contract proofs, tool prefixes,
operational jobs, package transactions, release/authenticated artifacts and
installed predecessors. Unknown material stays held. Age alone never permits
pruning; a passed receipt alone does not establish terminal ownership, complete
payload identity, archive coverage or successful recovery.

```sh
make retention-audit
make test-retention-audit
python3 -B scripts/retention_audit.py --path data/experiments/contract-proofs
python3 -B scripts/retention_audit.py --path build/my-profile --wall-cap 15
```

Make audits the selected build, test, experiment and tools roots through the
control-only route. The direct CLI selects generated/retained checkout namespaces,
with per-root bounds of 100,000 observed entries and five seconds by default.
Directory enumeration is bounded before materializing entries. Symlink leaves
are counted without following them; symlink roots/components are refused.
Special files are counted and held without being opened. Metadata reads are
strict and limited to 64 KiB. Larger or invalid marker metadata remains explicitly
unverified. Exit 2 means incomplete or unreadable inventory, not permission to
relax holds. Use an explicitly selected narrower path or bounded longer scan.

Inventory reports logical file bytes, not allocated storage or bytes reclaimable.
Hard links may count more than once. It is a non-atomic snapshot; concurrently
changing contents are not a verified deletion plan. Declared classes and legacy
retention signals are observations, not ownership proof. A container can include
other unknown material even when known classes are observed. All pruning flags
remain false, all archive/integrity/terminal-ownership checks remain unverified,
and no files, locks or generated roots are created. This tool has no apply mode.

Retirement routes are class-specific:

- Disposable builds require the existing exact owned `clean-plan`, cooperative
  exclusion and guarded clean. Mixed retained content remains held.
- Fixture scratch requires exact owner identity, terminal-state evidence and
  a separate retirement plan. Supervised fixtures now supply [bound terminal records](fixture_session_lifecycle.md);
  exact payload and external-owner retirement checks remain required.
- Retained evidence requires exact sealed inventory, independently covered
  payloads and recovery readback before any owner-approved pruning.
- Tool prefixes require explicit profile retirement and restoration at the
  correct prefix; they are not caches merely because they are generated.
- Operational jobs require owner reconciliation. Packages and installed
  predecessors retain their transaction/release/rollback authority.

Six behavioral tests verify class holds, no mutation, no-follow links, special
files, enumeration/metadata bounds, missing-root preservation and actual Make
routing without build fragments. The live Main Edit scan correctly held mixed
historical build storage, counted four environment symlinks and left oversized
legacy receipts unverified. Some roots reached inventory bounds. Its recorded
partial counts must not be presented as a complete workspace inventory.

This implements the inventory and policy portion of TL11 in the
[pilot lifecycle specification](top_level_lifecycle_spec.md). Explicit retirement,
archive-coverage matching, recovery qualification and pruning execution remain
open; the read-only audit does not substitute for those requirements.

Exact sealed-evidence coverage planning is now available in
[evidence_retirement_plan.md](evidence_retirement_plan.md). It links the copy and
restore receipts to complete selected current/restored inventories. Coverage
plans remain held: terminal ownership and guarded pruning are still required.
