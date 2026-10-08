# Retained notarization journal

Current integration: Main Edit's release Make entrypoints now use the
[receipt-bound release pipeline](release_pipeline_lifecycle.md). Cutover-pending
statements in the slice history below describe their recorded earlier state;
downstream consumer adoption and real-platform qualification remain open.


`release_notary.py` is a source foundation for the complete release-pipeline
cutover. It requires a completed notary-archive preparation transaction bound to
a completed Developer ID signing transaction, the exact signed app, archive and
receipt checksums. It does not grant release-control authority or prove a public
release. The legacy Make notarization recipe is not yet replaced.

The local installed `xcrun notarytool submit --help` and `info --help` confirm the
command forms used here. The helper uses keychain-profile authentication and JSON
output. Submit uses `--no-wait`; reconciliation is an explicit same-ID `info`
request, with no background polling or blind duplicate submission.

## State and failure contract

The journal writes and synchronizes submission intent before calling the tool.
Every command retains its own exclusive stdout/stderr under a UUID query folder.
The journal holds an input identity binding the archive, preparation receipt,
tool bytes, source helpers, profile name and exact signing/archive checksums.
Protected paths, unknown predecessors, changed identities and competing owners
hold before submission. Cleanup is excluded throughout each operation.

A successful submit captures the UUID and enters `submitted`; it never treats a
submit response alone as acceptance. Reconciliation queries that exact UUID.
Only an explicit `Accepted` info response for the same ID and unchanged input
binding enters `accepted`. Invalid/rejected and in-progress states remain
separate. Reuse rechecks the retained Accepted response as well as predecessors.
UUID casing is normalized; malformed JSON, duplicate fields, unknown status,
wrong IDs and unsafe response paths fail closed.

If a failed command left a complete submit response, reconciliation can retrieve
its ID from that exact retained stdout and query it without another submit.
If no trustworthy ID survived, the attempt stays held. Unverified process
termination also holds reconciliation. There is no manual ID-guessing override,
automatic history matching or retry that resubmits an uncertain archive.
A crash after durable intent can require owning release-control recovery even if
the command did not reach the service; conservative uncertainty is explicit.

Journal transitions are mutable control state; command response folders remain
retained evidence. Tool wall and sampled combined-log bounds are 900 seconds and
1 MiB per request. Process control uses the existing bounded cooperative teardown
runner. This is a trusted local tool, not a credential sandbox, hard quota or
forced-death/escaped-descendant recovery guarantee. No direct password, API key
or Apple ID is accepted by this helper. Profile names are local selectors, not
release authorization.

## Remaining cutover

The [archive producer and acceptance gate](release_notary_archive_lifecycle.md)
now connect the source helpers from signing through bound stapling. The complete
pipeline still needs consistent Make app selection, final artifact and refresh,
retain rejection logs, define recovery for unverified termination/unknown IDs,
and prove all Make entrypoints with fake tools before cutover. Actual macOS
signing, archive validation, service acceptance, metadata provenance and public
release require their separate owning gates. Existing local receipts are not an
untrusted cryptographic authorization system.

## Validation

Twelve notarization tests use a fake `xcrun` tool and fixture archives. They cover
single-submit/same-ID reconciliation, durable intent, a complete response surviving
a failed submit, unknown IDs, rejection, in-progress status, profile and signed
payload drift, accepted-response tampering, teardown holds and unsafe journal
paths. Ten app-stage and 27 package-transaction regression checks also pass (49
checks total). `make test-release-notary` is a control-only target that does not
require a compiler or pkg-config. No Apple submission, credential access or real
product signing was performed.

Evidence: `data/experiments/lifecycle-validation/20261007-notary-journal-foundation`.

The [final artifact and bound refresh helpers](release_final_artifact_lifecycle.md)
now complete the source-helper chain through strict final verification,
transactional export and predecessor-preserving temporary-Desktop proof. Full
legacy Make/controller cutover and real qualification remain open.
