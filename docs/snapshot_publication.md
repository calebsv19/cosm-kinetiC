# Native 2D snapshot publication

Main Edit 2D snapshot export now reuses the existing admitted native persistence
adapter rather than truncating its selected final path. The app-owned adapter
has an additive explicit byte-bound entrypoint (positive, at most 8 GiB); ordinary
settings begin remains bounded at 16 MiB. Snapshot export selects the 8 GiB class
bound, validates positive shape/buffer presence and finite time, and preflights
cell/header/payload arithmetic before allocating a pending attempt. It requires
exact expected staged byte count after checked writes/flush before publication.

The same cooperative parent lock, nofollow path/predecessor admission, changed
parent/lock checks, sync/close and atomic publication are reused. Partial write or
prepublication flush/close/sync/rename failures hold and retain pending output
without replacing the predecessor. A failed first export leaves the final path
absent. Post-rename unconfirmed semantics remain those in native_persistence.md.
No parent directory creation, rollback deletion, pruning or solver rerun occurs.
Existing scene-controller export callers already report false and mark a frame
exported only on true.

The binary PS2D version-1 format is unchanged: native-endian uint32 magic, version,
width and height, a double timestamp, then density/x-velocity/y-velocity float
arrays in the original order. This is legacy format preservation, not a new
portable interchange format or conversion to core_pack. No physical field,
solver algorithm or default was changed. Caller-owned buffer capacity and backend
integrity remain trusted beyond the checked shape/pointer/byte arithmetic.

Seven actual native snapshot methods pass: exact golden binary create/replace;
partial payload/flush/close/rename faults with predecessor and pending retention;
failed first export; oversized shape/nonfinite time before staging; linked/FIFO
refusal; a 20 MiB sparse predecessor admitted by the snapshot class rather than
the metadata class; missing/linked parent and invalid competing-lock holds.
Fifteen native persistence methods now include explicit bound tests (zero/overmax
no allocation, oversized completed candidate retention). Twelve configuration
JSON and four option-persistence regressions pass: thirty-eight distinct methods
total. The actual isolated backend Make contract passes in
`build/profiles/snapshot-publication-20261007`.

A retrospective native comparison compiles the retained writer-discovery backend
under the same controlled field fixture. Previous and current exporters produce
identical successful SHA-256 d2c8316a2be18e4c9d538e62d66f9836b8f3cfeb864c696df1f3da353a992a28.
An injected partial payload write destroys the prior final bytes with the old
exporter, but preserves them with the repaired exporter. Baseline source SHA-256:
1f27e2d03712d9c7fd2b28cc06886efc780545e915cf444f4e3af8b0849cda05.
Probe include setup initially lacked core/shape/timer headers used by the actual
Make graph; it was corrected and those failure logs are retained. No backend
implementation was changed to make the test link. Probes use the configured
fisiCs include root or the current workspace fallback; they are local developer
proof, not external-agent first-start commands.

The byte cap is admission/publication policy, not disk reservation, streaming
quota or a hard I/O deadline. Full hostile path-swap confinement, authenticated
writer ownership, pending snapshot recovery and exact retirement eligibility
remain incomplete. Large real snapshot campaigns, cross-host/native-endian
interoperability, GUI and installed/package behavior are not qualified here.

Reuse classification: existing PhysicsSim staged publication adopted; core_io's
minimal whole-file replacement lacks this ownership/retention contract, and
core_data/core_pack own data/format semantics rather than this legacy app export
policy. No shared API/version or minimum changed. No commit, canonical adoption,
cleanup, pruning, package, installation or release occurred. This local packet
is outside both frozen backup scopes. General text writers and coherent scene
cache replacement, plus the broader lifecycle requirements, remain open.

Evidence: `data/experiments/lifecycle-validation/20261007-snapshot-publication`.
