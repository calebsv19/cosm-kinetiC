# Retained release app transformations

Current integration: Main Edit's release Make entrypoints now use the
[receipt-bound release pipeline](release_pipeline_lifecycle.md). Cutover-pending
statements in the slice history below describe their recorded earlier state;
downstream consumer adoption and real-platform qualification remain open.


`release_app_stage.py` provides the copy-on-transform foundation for a coherent
release pipeline. It is source-level preparation, not a release authorization.
The existing `release-sign`, notarization, staple and signed-artifact Make
recipes have not yet been cut over. Do not treat this helper's passing tests as
completion of those entrypoints or authentication of a real package.

## Current source behavior

Each signing or stapling stage is a package transaction with a separate selected
root. It admits the source bundle ID, three exact regular executable files and
Frameworks directory before allocating output. Outputs and input must not
contain one another; protected, aliased, reused and unknown output paths hold.
The original app remains an input throughout copying, command execution,
verification and no-replace publication.

Signing enumerates regular dylib payloads from the admitted inventory. Internal
symlink aliases remain links. Spaces and duplicate basenames keep their argument
boundaries. Libraries are signed before the three executables and outer app.
The ad-hoc timestamp policy and Developer ID runtime options retain their prior
intent. Every command stops on failure; deep strict verification must succeed
before publication. Stapling works on another copy and validates it before
publication. Retry/resumption policy for actual notarization remains future work.

On macOS, staging, directory publication and interrupted-publication recovery use
bounded `/usr/bin/ditto --rsrc --extattr --acl` copies. Transaction identity binds
the copy tool. Recovery refuses missing directory publication on macOS without
that binding; old evidence is retained, not rewritten or implicitly upgraded.
Regular-file archives retain the existing copy behavior. Resource-fork and
extended-attribute preservation is tested on fixture app binaries, including
recovery. Inventories still bind ordinary bytes, modes and internal links; they
do not yet fully bind every extended attribute, ACL or authentication state.
Actual macOS bundle authentication and complete metadata provenance remain open.

Commands retain exclusive stdout/stderr files. Inner tool commands have 300-second
wall and 1-MiB sampled combined-log bounds; the outer transaction retains its
1800-second assembly and 64-MiB sampled-log bounds. Inventory limits apply per
identity pass. These are cooperative bounds, not hard disk/RSS quotas or a
sandbox. Each owned command group receives a bounded cooperative teardown window
before forced termination, allowing nested supervisors to reap their own groups.
Uncooperative or escaped descendants and forced-death recovery remain separate
gaps. A live SIGTERM fixture verified nested signing-tool termination.

Metadata-copy recovery journals now record terminal verification. Unverified
teardown holds retry and reuse. Valid verified interrupted publication retains
the original no-replace recovery behavior. Recovery inventories share budgets
across existing published outputs and recovery copies.

## Remaining coherent cutover

| Phase | Required input and output contract |
| --- | --- |
| Signing | Audited immutable packaged input → separate verified signed app and retained command reports |
| Notarization | Exact signed app/archive identity → retained submission attempt, explicit Accepted result and exact submission/archive binding; no blind duplicate submission after an uncertain interruption |
| Stapling | Accepted submission bound to the signed app → separate verified stapled app and retained validation reports |
| Final artifact and refresh | Exact stapled app → transactional portable archive/checksum/manifest; refresh from that app through predecessor preservation |

Before replacing legacy entrypoints, finish final artifact/refresh routing and
verify the full fake-tool Make pipeline. The [archive producer and acceptance
gate](release_notary_archive_lifecycle.md) now connect signing, retained
notarization and bound stapling in the source helpers. Real authentication, Registry and public-release acceptance require the
owning release-control decisions. No such decisions or external operations were
performed in this slice.

## Local validation

`make test-release-app-stage` uses only controlled fake signing/stapling tools
and temporary fixture apps, including real local macOS metadata-copy mechanics.
The control-only Make route works with compiler and pkg-config unavailable.
Ten stage tests passed. A further 73 affected transaction, command-supervision,
package-proof, unsigned-artifact and release-audit checks passed (83 total).
No real product signing, notarization, installation or publication occurred.
Evidence: `data/experiments/lifecycle-validation/20261007-release-app-stage-foundation`.

The [retained notarization journal](release_notary_lifecycle.md) now provides
source-level exact archive/signing binding, durable submission intent and same-ID
reconciliation. The notary-archive producer and acceptance-to-staple binding are now tested;
complete legacy recipe cutover remains open.

The [final artifact and bound refresh helpers](release_final_artifact_lifecycle.md)
now complete the source-helper chain through strict final verification,
transactional export and predecessor-preserving temporary-Desktop proof. Full
legacy Make/controller cutover and real qualification remain open.
