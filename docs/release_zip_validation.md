# Bounded ZIP payload verification

Current integration: Main Edit's release Make entrypoints now use the
[receipt-bound release pipeline](release_pipeline_lifecycle.md). Cutover-pending
statements in the slice history below describe their recorded earlier state;
downstream consumer adoption and real-platform qualification remain open.


`release_zip_validation.py` verifies a ZIP against the app's exact ordinary
inventory without extracting or executing members. The notarization archive
producer checks it before publication, and notarization admission checks it
again before submission or reconciliation. A typed preparation receipt alone
cannot bypass the content gate.

The verifier requires every app directory, regular-file byte/hash/mode and
internal symlink target/type to match. Missing, extra, duplicate, foreign,
traversing, backslash and NUL-truncated paths hold. Encryption, unsupported
compression and special-file encodings hold. Stored and Deflate payloads are
read in chunks with CRC verification. Symlink lengths are checked before reads.

Python's ZIP reader can return matching bytes and CRC even when a Deflate end
marker is missing. A regression fixture exposed this. An independent bounded
raw-Deflate check now requires a valid complete stream and rejects trailing data.
Its work is included in the decompression budget; reports distinguish logical
expanded bytes from decompression work bytes.

Central-directory count, size and individual headers are inspected before
`ZipFile` allocates its member list. EOCD and ZIP64 end/locator bounds are checked;
ZIP64 local headers and a ZIP64 end record have controlled passing proof.
Defaults bound 100,000 entries, 64 MiB central bytes, 32 GiB decompression work,
8 GiB per file/archive, 16 MiB per AppleDouble payload and 120 seconds of
cooperative verification time. Smaller bounds may be selected; these are not
hard RSS quotas or deadlines on an uncooperative filesystem call. Inventory
admission/rechecks also use their existing per-pass bounds.

Native macOS `ditto` fixture archives confirmed internal symlink encoding and
`__MACOSX` resource metadata layout. AppleDouble payloads must correspond to a
known app member and have valid bounded headers, unique entries and in-range,
non-overlapping data regions. Their CRCs are checked. The verifier does not yet
compare complete extended attributes, ACLs or resource-fork identity against
the app; it explicitly reports `complete_metadata_identity_verified=false`.
Signature verification and actual release-service acceptance remain separate.

Ten ZIP checks and 55 affected archive, app-stage, notarization and transaction
checks passed (65 total). Coverage includes native resource metadata, large
Deflate payloads, ZIP64, forged central counts, deadline admission, malformed
Deflate, ordinary-byte/link corruption and unsafe members. The control-only Make
route works with compiler and pkg-config unavailable:

```sh
make test-release-zip-validation
```

All signing and notarization effect tests use fake tools. Native `ditto` ran only
on temporary fake app fixtures. No real product packaging, signing, submission,
installation, publication or canonical adoption occurred.

This closes ordinary ZIP-content validation in the source helpers. Final artifact
and refresh routing, complete legacy Make cutover, real platform/authentication
qualification and full metadata identity remain open.
Evidence: `data/experiments/lifecycle-validation/20261007-bounded-zip-payload-validation`.

The [final artifact and bound refresh helpers](release_final_artifact_lifecycle.md)
now complete the source-helper chain through strict final verification,
transactional export and predecessor-preserving temporary-Desktop proof. Full
legacy Make/controller cutover and real qualification remain open.
