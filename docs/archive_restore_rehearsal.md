# Independent prepared-backup restore rehearsal

The prepared CFD snapshot was retrieved from the existing Linux PC cold archive
and restored into fresh local identities on October 7, 2026. The mounted volume
UUID matched `2958c9ed-b77f-4c88-9214-f0aa162de000` before and after retrieval.
The exact remote source was:

`/mnt/cold_archive_500g/codework-archive/generated-runs/physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a/payload/`

Only the six receipt-bound payload files were retrieved through read-only SSH/SFTP.
The archive copy and original local/remote intake were not modified. The initial
fetch selected the batch root rather than `payload/`; the recorded inventory
resolved that path error, and the corrected retrieval matched all six checksums.

The local rehearsal root is:

`/private/tmp/physics-cfd-restore-9a1847e43e1046cbb3050a5ad3c82607/`

- Historical restoration: 6,391 files, 362,415,784 logical bytes, with the exact
  path/size/SHA-256 inventory matching `survivor-manifest.json`.
- Fresh restoration: 560 files, 77,675,322 logical bytes, including eleven sealed
  bundles whose restored byte inventories passed readback.

This proves recovery of the prepared snapshot, not restoration of files that
were missing before backup. Later hardening packets remain outside this payload.
No restored binary was executed and no solver/runtime/installed qualification is
inferred. Historical receipt claims remain immutable; status attaches the new
rehearsal only when its sealed record matches the exact source copy receipt hash,
archive destination and payload checksum map. General current-evidence coverage
remains false.

`restore_rehearsal.py` provides create-only local verification:

```sh
make test-restore-rehearsal
python3 -B scripts/restore_rehearsal.py \
  --payload /absolute/path/to/retrieved/payload \
  --receipt /absolute/path/to/exact-copy-receipt.json \
  --destination /absolute/path/to/new-empty-identity
```

The destination must not exist, even if empty, and must not overlap payload
storage. All six file checksums are verified before extraction. Tar extraction
refuses absolute/traversing paths, duplicate names, links and special files. Entry,
expanded-byte, per-file and wall bounds apply. Ordinary file permissions are
restored with special bits stripped. Each regular restored file is hashed after
writing, historical inventory must match exactly, and every restored sealed
bundle must verify. Partial failures are retained and cannot be silently reused.
These controls are not an untrusted-public-upload interface or full filesystem
metadata recovery (ACLs, extended attributes and runtime relocatability are not
qualified).

Five restore behavior tests, ten status tests and thirteen doctor regressions
passed. The real independently retrieved payload restored successfully. Evidence
is sealed in `20261007-independent-restore-rehearsal`; the retrieved archives and
expanded trees remain at the separate rehearsal root rather than being duplicated
inside that small validation packet. Retention/pruning eligibility remains a
separate decision.
