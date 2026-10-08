# Focused PhysicsSim cleanup closeout

Completed 2026-10-07 in the canonical source checkout at
`/Users/calebsv/Desktop/CodeWork/physics_sim`, base commit
`3fa1ad5f6fb2384a7626e83c5e22fa7821cb7ed5`. The user subsequently authorized
saving the cleanup source, documentation and retained verification records in
one Git commit; use repository history for that commit identity. No version
change, installed app refresh, signing, publication or Registry mutation was
performed. Earlier receipts describe the original uncommitted validation state.

## Completion evidence

1. Guarded cleanup and owned compilation are adopted. The default profile is
   `build/profiles/local-owned`; `make clean-plan` is read-only. A disposable
   build → clean → rebuild sequence and Water, scene-cache and CLI proofs passed,
   with retained experiment/tool/test sentinels, legacy build output and the
   older binary unchanged. A direct canonical headless build passed. The 18
   cleanup methods passed after the final read-only `make help` addition.
2. Independent preservation is complete for the exact frozen snapshot below.
   The approved bundle was uploaded, imported to the PC cold archive, retrieved
   into a fresh local directory and verified. All 16,475 files passed hash,
   size and mode checks. The helper verified the physical mount identity and
   reported copying without deletion. Before closure-only doc/receipt edits,
   all 1,396 canonical source files still matched the snapshot.
3. Both launchers publish verified private writable configuration. The macOS
   migration preserves the original package link; application saves in fixtures
   leave package resources unchanged. Incomplete Linux copies are held rather
   than accepted. All 37 selected shell-launcher methods passed.
4. `make help`, `cleanup_operations.md`, `launcher_configuration_recovery.md`,
   `packaging_lifecycle_operations.md` and the read-first headless docs agree on
   the selected profile, retained roots, fixture reset boundaries and recovery.
   This closeout supplies the final archive coverage and retrieval receipt.
5. Existing bundler and installer repairs and required transaction/proof helpers
   are integrated. The 181 affected methods passed; a native unsigned Mach-O
   fixture passed dependency-copy/rewrite readback. Platform limits below remain
   explicit and are not represented as passing acceptance checks.

## Exact independent backup

Host: `pc-wg10`. Disk UUID: `2958c9ed-b77f-4c88-9214-f0aa162de000`.

Stored payload:

```text
/mnt/cold_archive_500g/codework-archive/generated-runs/physics-focused-preservation-20261007d/20261008T050409Z--physics-focused-preservation-20261007d/payload/physics-focused-preservation.tar.gz
```

Compressed size: 43,174,272 bytes. Logical listed file bytes: 225,434,677.
SHA-256:

```text
d2546e02ba988902b7e0f665dfb41c465cff2da8e2dc2f9a9503cbf638998253
```

Coverage includes canonical and Main Edit present tracked/nonignored source
snapshots, all 167 Main Edit lifecycle packets, five canonical focused repair
receipts, the private documentation-sync log and restore metadata. The Main Edit
base is `dd55d7c0f4bbf6f7f4e614e5ee71ded7982b1962`; its missing tracked
`tests/test_cfd_reference3d_force_local.py` is recorded as missing, not restored.
No Main Edit work was reset or bulk-adopted. Of the 167 lifecycle packets, 135
pass the current CFD verifier, 24 have other/older manifests it refuses and eight
are unsealed. All are byte-preserved without changing their qualification.

The payload contains `coverage.json`, `RESTORE.md` and source-state/deletion
metadata. Restore into an empty directory and verify the exact regular-member
inventory, hashes, sizes and modes before using it. Do not extract over an active
checkout. This is a working-tree snapshot, not a full CodeWork workspace or Git
history. Shared sibling libraries, ignored build/package/runtime outputs,
reference environments and other experiment families are excluded. Closure-only
docs and transfer/retrieval receipts were written after this frozen snapshot;
the local receipt records those separately rather than claiming they are inside
the payload. Later source changes require another backup.

The earlier independently verified CFD archive remains retained separately:

```text
/mnt/cold_archive_500g/codework-archive/generated-runs/physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a
```

Local records:

- `data/experiments/backup-preparations/20261007-final-focused-preservation/current-readback.json`
- `data/experiments/backup-preparations/20261007-final-focused-preservation/coverage.json`
- `data/experiments/backup-preparations/20261007-final-focused-preservation/retrieved-verification.json`
- `data/experiments/focused-cleanup/20261007-completion-audit/`

## Deferred boundaries

The independently dirty Main Edit lane still needs deliberate reconciliation
before it can supersede the repaired canonical implementation. Real installed
profile migration, GUI acceptance, Linux desktop-file validation and signed
platform package acceptance remain separate work. Historical artifact retirement,
aggregate retention/quotas, full power-loss recovery and hostile concurrent path
replacement were not part of this finite cleanup. No retained history was pruned.
