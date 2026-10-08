# Final artifact transaction and bound refresh

`release_final_artifact.py` consumes a completed stapling transaction with exact
Accepted notarization lineage. It publishes a final ZIP, portable checksum,
manifest, notary sidecar and verification reports through the existing retained
package transaction. The Main Edit release Make entrypoints now use the coherent
[receipt-bound pipeline](release_pipeline_lifecycle.md). This helper grants no authentication, Registry or publication authority.

## Admission and verification

Before allocation, the exporter reproduces stapling source/output identities and
the accepted journal binding, including the exact original signed app. It checks
bundle ID, product name and short version against Info.plist (XML or binary), and
requires the PhysicsSim macOS platform identity. Protected/overlapping roots,
unknown predecessors and changed completed inputs are held.

Fresh bounded commands run strict deep codesign verification, stapler validation
and Gatekeeper assessment. Every nonzero Gatekeeper status is a failure here,
including the subsystem error previously ignored by the legacy recipe. `lipo`
checks the requested thin architecture for all three executables and every
regular dylib payload. Symlink aliases retain their existing payload relation.
The selected tool bytes are transaction inputs; successful fake-tool tests do
not establish real package authentication.

The archive tool runs only after these checks. The exact ordinary ZIP payload is
verified with the [bounded ZIP gate](release_zip_validation.md). Checksum and
manifest reference sidecar/artifact basenames. The notary sidecar distinguishes
the submitted signed-app archive checksum from the final stapled-app archive
checksum, and binds the Accepted submission ID and journal receipt checksum.
Sidecars are rechecked before no-replace publication. Partial archives, command
failures and publication failures remain retained. Exact matching completed
exports reuse without rewriting final outputs.

## Refresh selection

A refresh requires the completed final export receipt. Readback reproduces the
entire lineage, final archive payload, checksum/manifest/notary sidecars and the
exact stapled app selection. Shared cleanup and final-artifact owner locks cover
readback and replacement. The existing desktop replacement helper retains its
matching app-name/direct-Desktop boundary and predecessor/history/recovery rules.
It copies the selected stapled app, not the mutable original package root.
A post-replacement readback verifies that artifact selection remained unchanged.
Independent source-stage ownership and forced-death recovery remain part of the
broader lifecycle audit; no hard filesystem snapshot is claimed.

## Validation and limits

Seven final-export/refresh tests, fourteen Desktop preservation tests and 27
transaction checks passed (48 total). They cover portable sidecars, no-write
reuse, Gatekeeper subsystem failure, architecture/version mismatch, retained
partial archives, unknown predecessors, acceptance drift, bound app refresh,
predecessor retention and sidecar tampering before Desktop allocation.
The control-only target works with compiler/pkg-config unavailable:

```sh
make test-release-final-artifact
```

All authentication and notarization effect tools are fake; refresh uses a patched
temporary home/Desktop. No real Desktop, product signing, Apple submission,
installation or publication was changed. Native copy mechanics run only on
fixture apps. Full metadata/ACL/resource-fork identity, real binary/authentication
qualification, public/Registry readback and canonical adoption remain open.

The controller/Make source cutover now connects consistent phase roots, exact
receipt selection, saved-ID notarization reconciliation and final artifact/app
routing. Authenticated sidecars use the archive stem to match Release Control
reader conventions. Downstream consumer adoption and actual package qualification
remain open; existing sealed evidence below records the earlier helper slice.
Evidence: `data/experiments/lifecycle-validation/20261007-final-artifact-and-bound-refresh`.
