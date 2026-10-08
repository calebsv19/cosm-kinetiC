# Later PhysicsSim lifecycle backup — prepared, not transferred

A separate batch `physics-lifecycle-hardening-20261007b` is prepared for the
existing PC cold archive. It contains exactly 70 sealed lifecycle packets,
4133 regular files and 45,872,267 logical bytes. The compressed
payload is 8,001,051 bytes, SHA-256
`395fa4bc52e4d09e14fea400663001d38091806e47d7fd3914ad30d8e7b275d3`.

Selection excludes bundle identities already covered by the original restore
receipt. Each source bundle was verified before and after archive creation.
Empty-destination local readback matched the exact member set, every file byte
count/checksum and all seventy restored bundle manifests. Two retained service
lock files are included as inert bytes; archive restoration does not confer their
runtime ownership. This is a scoped evidence snapshot, not a complete live
worktree, binary/environment backup or CFD qualification.

Coverage includes later build/clean ownership, setup, package/release-source,
session/atmosphere and native output/metadata/log/startup hardening packets,
including the original archive copy/restore receipts. `coverage.json` is the exact
file and bundle inventory. Later preparation/approval receipts and future changes
are outside this cutoff. Original backup identity and all source data remain
unchanged. No cleanup or pruning was performed.

The supported export-dropbox helper staged item
`20261007T162116Z--physics-lifecycle-hardening-20261007b` locally. Automatic approval
review rejected its upload: the earlier approval covered the specifically prepared
original backup, while this later internal evidence payload requires separate
human authorization. No network upload was started. Do not route around this
rejection. Independent-copy and archive-retrieval verification remain false.

Proposed action after explicit approval: upload only this exact staged item to
`/srv/codework-inbox/export-dropbox/` using the supported helper; run the existing
cold-archive controller's import dry-run and checksum-verified copy into a new
`generated-runs/physics-lifecycle-hardening-20261007b` batch; fetch/read back the
same archived payload and verify it into a fresh empty restore destination.
Preserve the earlier archive, intake and all originals. Any changed payload needs
fresh scope review rather than this approval.

Prepared evidence: `data/experiments/lifecycle-validation/20261007-later-archive-prepared`.
