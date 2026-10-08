# Notary archive production and acceptance-to-staple binding

Current integration: Main Edit's release Make entrypoints now use the
[receipt-bound release pipeline](release_pipeline_lifecycle.md). Cutover-pending
statements in the slice history below describe their recorded earlier state;
downstream consumer adoption and real-platform qualification remain open.


The source release helpers now connect Developer ID signing evidence, a retained
notary archive, the exact notarization journal and copy-on-transform stapling.
The legacy Make release recipes are still pending coherent cutover; no actual
product signing, Apple submission, installation or public release ran here.

## Archive preparation

`release_notary_archive.py` requires the completed signing transaction and its
exact app output. Ad-hoc signing, changed predecessors, overlapping/protected
roots and unknown existing outputs hold before archive creation. The producer
runs the selected archive tool inside `package_transaction.py`, preserving the
signed app and retaining every partial archive attempt. It publishes ZIP,
basename-only checksum, JSON manifest and command reports together through the
existing no-replace transaction/readback rules. Matching completed attempts
reuse without replacing outputs; failed attempts require a new owning job root.

The archive transaction declares the exact signed app and signing receipt as
inputs, including the receipt checksum. This is the producer contract consumed
by `release_notary.py`; tests no longer need a synthetic producer to exercise the
archive-to-staple path. The source producer now performs [bounded ZIP payload verification](release_zip_validation.md),
including ordinary content, modes, links, CRC, Deflate completion and native
AppleDouble structure. The same gate runs at notarization admission. Fake test
tools produce valid fixture ZIPs; the apps and authentication remain controlled
fakes. Full metadata identity and real archive/authentication qualification remain open.

## Stapling gate

The notarization journal now records the exact archive and preparation-receipt
paths. `accepted_binding` revalidates their transaction lineage and checks that
the accepted signed app is precisely the stapling input. It also verifies the
recorded input identity and the retained same-ID Accepted response.

Both outer stage admission and direct inner assembly require this evidence
before output allocation or tool execution. Stapling binds the receipt, response,
archive and preparation receipt as transaction inputs. It rechecks acceptance
after transformation, and the published stage metadata retains the accepted
submission ID and exact archive/signing checksums. Missing, tampered, drifted or
other-app acceptance is held. A passed helper does not grant external authority
or convert the local evidence into a public/Registry acceptance claim.

## Validation and remaining work

Six producer/gate checks, ten updated app-stage checks, twelve notarization
checks and 27 transaction checks passed (55 total). Coverage includes portable
sidecars and no-write reuse, retained partial archive failure, ad-hoc/protected
root holds, exact-app acceptance, response tampering and unknown predecessors.
The archive target is control-only and runs with compiler/pkg-config unavailable.
All signing, archive and notary effect tools in these tests are controlled fakes.
The existing macOS metadata-copy fixture still uses local native copy mechanics.

Next is the controller that connects the actual Make entrypoints consistently:
sign a separate app, prepare the exact notary archive, submit/reconcile the saved
ID, staple another copy, then transactionally export and refresh that final app.
That controller must preserve pending/uncertain states and must not create a
new submission on repeated invocation. Final artifact routing/qualification and real
platform/authentication qualification remain open. Canonical adoption, release
publication and later independent backup coverage remain open.

Evidence: `data/experiments/lifecycle-validation/20261007-notary-archive-and-staple-binding`.

The [final artifact and bound refresh helpers](release_final_artifact_lifecycle.md)
now complete the source-helper chain through strict final verification,
transactional export and predecessor-preserving temporary-Desktop proof. Full
legacy Make/controller cutover and real qualification remain open.
