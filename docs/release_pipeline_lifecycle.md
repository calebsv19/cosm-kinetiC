# Receipt-bound release Make lifecycle

Main Edit's signing, notarization, stapling, final artifact and release Desktop
refresh entrypoints now use the retained source helpers. This is source mechanics
hardening. Release Control and Production Registry retain effect authority; these
commands do not grant authentication, installation or publication permission.
Canonical adoption and real-platform qualification remain separate work.

## Phase selection and preserved inputs

`release-sign` and its `release-verify` / `release-verify-signed` aliases create and
verify a separate signed app. They preserve the packaged source app. Ad-hoc sign
is allowed only for the sign phase. Product, version, bundle identity, supported
platform/architecture identity and exact tool selections are admitted before
signing. The final artifact checks the actual binary architecture.

`release-notarize` prepares a verified upload ZIP from that signed transaction,
then records submission intent before invoking the tool. A new submission is
followed by one same-ID status query. Pending or rejected status stops downstream
work. A repeated invocation reconciles the retained journal and never submits
again. Unknown submission identity or unverified process teardown stays held;
there is no background polling or guess from service history.

`release-staple` consumes only exact verified Accepted evidence, copies the signed
app into a separate transaction and verifies stapling. `release-artifact` and
`release-distribute` produce the final ZIP and portable sidecars only after all
previous phases, strict codesign, stapler validation, successful Gatekeeper
assessment, architecture checks and bounded ordinary ZIP payload verification.
Gatekeeper errors are no longer ignored by these entrypoints.

`release-desktop-refresh` retains the existing authority prerequisite and requires
`RELEASE_FINAL_RECEIPT` identifying a completed final artifact transaction. It
selects the app bound to that transaction and preserves the installed predecessor.
It does not pick the newest transaction or accept a raw packaged app.

## Paths and caller contract

The default authenticated root is `$(RELEASE_DIR)/authenticated`, overridable by
`RELEASE_PIPELINE_ROOT`. Its children are `signed`, `upload`, `notary`, `stapled`
and `final`. Every transform/export has its own exact transaction receipt. The
notary journal is `notary/receipt.json`. Completed phase commands print their
receipt as JSON; callers must retain it, including the final receipt for refresh.

The authenticated artifact is
`final/$(RELEASE_ARTIFACT_BASENAME).zip`, with an adjacent `.zip.sha256` checksum and stem-based
`.manifest.txt` and `.notary.json` sidecars. This
intentionally differs from the unsigned `RELEASE_APP_ZIP` / `RELEASE_MANIFEST`
paths. `release-local-artifact` keeps its existing unsigned contract. The Registry
contract inspected for PhysicsSim selects `release-local-artifact` for local
macOS preparation; this cutover does not change that contract. Other release
consumers must use exact authenticated receipts before adopting this lane.

Existing roots are never repurposed to a different identity. Unknown predecessors,
input drift, failed stages and uncertain operations stay retained. A clean root
requires a new owner-selected namespace, rather than deleting previous evidence.

## Verification and limits

The pipeline fixture exercises the full sign/archive/submit/info/staple/export
flow and the Make entrypoint with fake tools, temporary app roots and disabled
real package prerequisites. It verifies source preservation, exact reuse without
additional effects, same-ID pending recovery, rejection stopping downstream,
unknown predecessor preservation and required refresh receipt admission.

Both `macOS` (the native Make contract) and historical `macos` platform spelling
are admitted and preserved in artifact identity. Stem-based sidecars match the
current Release Control reader convention.

Source tests are not signature trust, Gatekeeper platform qualification, Apple
acceptance, public publication or installed-product acceptance. Inventory and ZIP
bounds are cooperative per pass; full xattr/ACL authentication identity, hard
workflow resource limits and forced-death recovery remain open. No real release
or Desktop mutation is performed by these tests.

Final validation: six pipeline checks passed through the control-only Make target
with unavailable compiler/pkg-config, and seven final-artifact regression checks
passed after platform/sidecar alignment (13 checks total). `release-contract`
readback reports distinct unsigned and authenticated paths using native `macOS`.
Evidence: `data/experiments/lifecycle-validation/20261007-release-make-cutover`.
