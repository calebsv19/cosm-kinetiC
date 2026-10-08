# PhysicsSim top-level stability assessment — October 7, 2026

The prepared backup is complete. Main Edit has substantially stronger setup,
build and cleanup controls, but repository-wide lifecycle completion and canonical
adoption remain open. Canonical is still at 3fa1ad5; Main Edit is an uncommitted
implementation lane at base dd55d7c. Preserve unrelated CFD work during adoption.

## Backup coverage

The existing PC cold-archive controller copied the prepared 303 MiB snapshot to
`/mnt/cold_archive_500g/codework-archive/generated-runs/physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a`.
Six payload checksums match. The later independent retrieval/restore rehearsal
matched all 6,391 historical files (362,415,784 bytes), plus 560 fresh files and
11 sealed evidence bundles. Originals and remote intake remain intact.
Both sealed receipt bundles were reverified for this assessment. Later hardening
packets are outside that prepared backup; missing pre-incident evidence is not
recovered by it. Restore integrity does not establish numerical correctness.

## Current controls and remaining gaps

| Area | Verified local coverage | Remaining improvement |
| --- | --- | --- |
| Clean | Main Edit plans every declared disposable path, refuses protected/unknown/changed evidence and checks ownership before mutation | Canonical still uses broad recursive removal; finish writer inventory before repository-wide claims |
| Build isolation | Selected executable roots, configuration selection, real no-op/rebuild checks, atomic publication and cooperative ownership | Full external header/library identity, legacy direct writers and forced-termination recovery |
| Tests and reports | Fresh integration capsules, frozen inputs and bounded logs; migrated native/semantic/report contracts retain failures | Historical numerical writers and operational-job lifecycle still need classification |
| Setup | Read-only status/doctor; matching pinned reference environments reused without reset; incomplete profiles held | Other tool families and full transitive dependency provenance |
| Packages | Source helpers have predecessor-preserving transaction/recovery tests | Remaining signing/export paths and real platform/package qualification |
| Retention | Distinct artifact classes and bounded read-only audit; exact prepared backup and restore coverage | Exact retirement eligibility, coverage matching, interruption readback and guarded pruning |
| Adoption | Pilot specification records thirteen explicit requirements and gaps | Bounded reviewed diff, reconciliation of dirty CFD work, canonical first-start proof |

## Current assessment checks

Read-only headless and CFD-reference doctor checks passed layout, source,
metadata, Python, compiler, Make, pkg-config, target, SDK and dependency checks.
Both report attention_required: the original build contains retained evidence,
selected default binaries/profile are not qualified, and later backup coverage
is incomplete. These holds are expected. No clean was applied.

The latest mixed-refinement recipes now retain fresh 8/16/32 native and matrix
JSON series. Matrix assessment builds its own fresh native companion rather
than conditionally consuming a shared historical file. Real native and coupling
recipes passed after the interpreter-path repair; ten existing contract regression
checks passed. Explicit reference-venv invocation is preserved, while the default
interpreter resolves its physical path to avoid rejecting Homebrew alias parents.
The optional venv configuration is copied and hashed. Dedicated new-series
failure/drift tests remain to be added before broad lifecycle qualification.
Numerical assertions and solver source were not weakened by this migration.

The historical 487-test reference pass and public Water/scene-cache proofs remain
recorded evidence; they were not rerun in this assessment. These controls do not
certify full CFD force/transient accuracy, installed-product state or a release.

## Recommended order

1. Prepare a bounded canonical adoption diff for the cleanup/control foundation,
   reconcile pre-existing CFD ownership, and run the supported first-start proofs
   on that exact adoption candidate. Until then, use Main Edit with an isolated
   build root; preserve the held original tree.
2. Audit each supported writer and reset command against the pilot requirements.
   Finish fresh output identities, direct execution ownership and failure behavior;
   classify historical workflows explicitly instead of guessing they are scratch.
3. Complete exact archive coverage and retirement plans before pruning. Archive
   later evidence in a separately identified batch; keep prepared backup identity
   immutable. Prove retrieval and interruption readback for each eligible class.
4. Finish transitive build/environment provenance and forced-death recovery;
   qualify signing/export and package refresh under their owning release workflow.
5. Reconcile the accumulated progress docs into one current operator contract,
   then use the proven specification as the template for other CodeWork programs.

Evidence: `data/experiments/lifecycle-validation/20261007-top-level-assessment-current`.
Specification: `docs/top_level_lifecycle_spec.md`. Detailed historical findings:
`docs/build_cleanup_assessment.md`. No survivor deletion, canonical sync, commit,
package build, install or publication occurred in this assessment.
