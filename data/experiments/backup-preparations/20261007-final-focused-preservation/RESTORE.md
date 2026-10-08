# PhysicsSim focused preservation snapshot

Extract into an EMPTY directory, never over an active checkout. Accept only the
regular archive members listed in coverage.json, plus coverage.json itself.
Reject absolute paths, parent traversal, links, devices, duplicates and extras.
Verify each file's SHA-256, byte size and mode against coverage.json.

source/canonical and source/main-edit contain present tracked and nonignored
source files selected with git ls-files. They are independent working-tree
snapshots, not commits and not a runnable full CodeWork workspace. Consult
metadata/source-states.json for base commits, status, exact missing/deleted
paths and exclusions. Do not restore a missing tracked path from a base commit
without reviewing the recorded deletion. Shared sibling libraries, Git history,
ignored build/package/runtime outputs, reference environments and other
experiment families are outside this snapshot.

evidence/main-edit-lifecycle preserves every regular file from the selected
167 lifecycle packets. metadata/packet-verification.json records current CFD
verifier results: alternate/older manifests and unsealed packets are preserved
as-is, not promoted to verified CFD bundles. The outer SHA inventory protects
their bytes. evidence/canonical-focused-cleanup preserves all five adoption
receipts. Later changes after this frozen snapshot require another backup.

The earlier independently retrieved CFD archive remains separate and must be
retained at /mnt/cold_archive_500g/codework-archive/generated-runs/
physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a.
The earlier 20261007b local staged bundle was not uploaded; this snapshot adds
later source and receipts without overwriting that local preparation.

A local restore proves archive integrity only. Independent backup is complete
only after the stored PC cold archive payload is retrieved and verified.
