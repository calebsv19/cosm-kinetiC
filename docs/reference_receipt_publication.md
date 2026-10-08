# Retained transactional reference receipt publication

All 194 reference wrappers now publish their final receipt through an owned
create-only transaction instead of writing JSON directly to the final path.
The caller must hold verified reference namespace descriptors covering that path.
The complete encoded JSON is checked under the existing 16 MiB, depth/event and
finite-number admission before attempt allocation.

Each publication allocates a fresh `.receipt-attempts/<uuid>/` containing a
create-only request (destination basename, exact bytes and SHA-256, no replacement)
and a receipt candidate. Request/candidate files are flushed and fsynced. Candidate
bytes are rechecked and attempt directories synced before an atomic no-replace
hard link publishes the final pathname. The parent directory is synced and final
bytes rechecked before successful return. Candidates and requests remain retained.

An existing or raced destination cannot be replaced. Failed writes/syncs or link
collisions retain their attempt and never delete an existing receipt. Before-link
failure leaves the final path absent. A failure after linking can leave a complete
published receipt with unconfirmed acknowledgement; callers receive the hold,
and later readback must verify actual bytes rather than assume success. No
rollback/delete, automatic rerun or interrupted-prefix reset is introduced.
Candidate and final are hard links to the same complete bytes, so independent
backup packaging must materialize regular-file bytes under its own verified
archive contract. This transaction is integrity/preservation, not authentication.

Behavior proof includes exact successful candidate/request/readback, predecessor
preservation, raced final creation, candidate fsync failure, nonfinite/deep/oversize
JSON before allocation, linked attempt storage and required verified ownership.
Existing JSON/artifact budgets and expected inventory checks remain in force.
Family control tests exercise fresh success/failure and immutable cached readback
across all wrappers. Compiler/numerical command assignments compared unchanged.
Controlled native libraries and synthetic numerical probes do not qualify real
CFD ABI or physical accuracy.

This is a single-record transaction. Coherent multi-file operational recovery,
full descendant lifetime, workflow/storage quotas, terminal-owner eligibility,
archive-backed retirement/pruning and canonical adoption remain open. No commit,
installation, release, backup transfer or original-data cleanup occurred.

Evidence packet: `data/experiments/lifecycle-validation/20261007-reference-receipt-publication`.
