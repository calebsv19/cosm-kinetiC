# PhysicsSim setup and cleanup assessment — October 7, 2026

The approved prepared backup is complete and independently retrieved/restored.
Main Edit has substantial verified lifecycle protections, but PhysicsSim remains
in transition: canonical has not adopted them, specialized writers remain, and
later hardening evidence is outside the prepared snapshot.

## Backup and recovery

The existing archive system holds the six prepared payload files under:
`/mnt/cold_archive_500g/codework-archive/generated-runs/physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a/payload/`.

The original completion receipt records matching source/destination checksums
and sizes. A subsequent independent retrieval verified all six payload hashes,
restored 6,391 historical files matching the survivor manifest, and restored 560
fresh files with eleven sealed bundle readbacks. Mount identity was checked
before and after retrieval. Originals and remote intake remain retained; no
pruning occurred. Missing pre-incident files remain missing. Restored binaries
were not executed, so this is recovery/readback evidence, not runtime acceptance.

This assessment freshly reverified the sealed copy packet (22 files, manifest
SHA-256 `90d8a15cb2d201236b5cc8f5106dfcf912b278c641c8eb48c4b13f8199340e99`)
and restore packet (23 files, manifest SHA-256
`6b12a439b4d657ba81a15273f4e512893187924e5911a40a095749c715b5c735`).
It did not repeat the completed transfer. Later hardening packets are not covered.
See [restore rehearsal](archive_restore_rehearsal.md).

## What is stronger in Main Edit

- Guarded clean plans the whole selection, refuses unknown or changed output,
  protected roots, symlinks and retained evidence, and revalidates before deletion.
- Cooperative build ownership excludes overlapping builds and cleanup. Selected
  build profiles isolate ordinary binaries; staged compiler publication preserves
  predecessors on failure. Configuration/tool identity drives rebuild selection.
- Twenty integration scripts allocate fresh capsules and use lifetime supervision;
  bounded public first-start and job-runner checks passed. Heavy/private fixtures
  retain separate qualification requirements.
- Status and doctor inspect prerequisites and holds without installation. Reference
  setup reuses exact matching environments without writes and holds incomplete ones.
- Retained numerical, semantic and contract proofs have distinct lifetimes. Package
  transaction/replacement helpers preserve predecessors and failed attempts in
  source tests; real platform, installed and release acceptance remains separate.
- A versioned retention policy and bounded read-only inventory exist.

Fresh read-only status found one verified backup and one matching restore
rehearsal, no metadata diagnostics, and a hold on the original build because it
contains retained CFD runs. All-current-evidence backup coverage remains false.
No original tree was cleaned.

## Improvements in recommended order

| Priority | Improvement | Required outcome |
| --- | --- | --- |
| 1 | Finish specialized writer coverage | Audit direct compiler recipes and standalone writers. Replace fixed build paths with selected roots and ownership/atomic publication for disposable binaries; use fresh sealed identities for evidence-producing runs. Ordinary refined CFD and pressure/material/atmosphere contract binaries are migrated; atmosphere workers and consumers now share selected paths; evidence-producing refined/convergence recipes still contain fixed-path writers, and standalone execution ownership needs audit. |
| 1 | Adopt the bounded contract into canonical | Separate lifecycle changes from pre-existing CFD work, review the adoption diff, reconcile docs and run supported first-start proofs. Canonical currently still runs broad recursive deletion in its clean recipe. |
| 2 | Complete retention and guarded pruning | Explicit eligibility, terminal owner state, exact archive coverage and readback must precede retirement. Age alone is insufficient. The current inventory reports holds; it is not a pruning tool. |
| 2 | Extend archive coverage | Prepare a separately identified snapshot of later hardening evidence and source needed to reproduce it. Keep prepared-snapshot coverage distinct and rehearse recovery for each new snapshot. |
| 2 | Finish package/export/refresh coverage | Audit remaining signing, export and script writers under their owning authority, then qualify actual platforms separately. Local source tests cannot establish installed or public product freshness. |
| 3 | Complete setup/dependency provenance | Extend bounded doctor/bootstrap contracts to remaining tool families and transitive library inputs; keep absent prerequisites actionable and avoid in-place environment repair. |

The desired command surface distinguishes status/doctor, clean-plan, disposable
build cleanup, explicit fixture retirement, archive audit and recovery. Cleanup
must not erase retained runs, operational job state, reference environments,
package recovery data or authenticated artifacts.

The [top-level lifecycle specification](top_level_lifecycle_spec.md) records
thirteen acceptance requirements and their remaining gaps. This assessment does
not certify repository-wide completion, CFD physical accuracy or release state.
Changes remain uncommitted in Main Edit. No canonical synchronization, package
build, installation or release occurred in this assessment.
