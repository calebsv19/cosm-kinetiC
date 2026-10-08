# Scene cache staged-copy integrity

A controlled native fwrite fault changed one copied raw density value to a
different finite value without changing file size, header or mask. The prior
publisher accepted it and displaced the predecessor. Finiteness, mask hash and
structural inventory admission therefore did not establish source/copy equality.

Publication now compares complete staged bytes against the current admitted
source for all four output directories after staging and semantic validation,
before the first predecessor rename. Selected manifests, scene bundle, raw
VF3D, optional opaque packs and water sidecars use the same check. Source-selected
filenames must exist in the staged tree; staged counts must match and no
unselected staged entries are accepted. A selected source directory/special file
fails rather than being silently omitted.

Files are admitted as regular, single-linked, no-follow entries, checked for
matching size, streamed with two fixed 8 KiB buffers and exact pread/EINTR loops,
and checked for EOF and named identity at completion. Both directory witnesses
are rechecked. Each file is at most 8 GiB; all four comparisons share a 32 GiB
logical-pair-byte limit and sampled 120-second monotonic budget. A pair consumes
one logical file size while reading twice that size; this is not an aggregate
workflow physical-I/O or copied-byte quota. Empty optional files compare normally.
A mismatch retains staged output and all predecessors behind the existing hold.

Two new native methods cover finite frame mutation and six equal-length pack or
semantically equivalent JSON mutations across all four staged directories. Tests
verify all seven predecessors remain, source bytes remain unchanged, differing
staged bytes remain available, and the attempt stays held. Existing short/EINTR,
mask, structural, recovery and compiler-profile coverage also runs.

Reuse scan: core_scene_compile exports a one-shot SHA-256 API; its incremental
context is private. core_data/core_pack have no suitable bounded streaming digest
API. Allocating an up-to-8-GiB file for one-shot hashing would violate the bounded
reader contract. The app-specific copy check therefore reuses its existing
bounded reader and entry witnesses (`reuse-deferred` for a durable digest).
No shared module/API/version, source cache bytes, binary format or solver changed.

This proves equality to the currently observed source, not cryptographic source
authentication or binding to the initial source admission. It does not freeze all
source/staged entries across the entire plan, eliminate hostile races after the
last witness, qualify pack semantics, or make seven-slot visibility atomic.
Durable digest-bound inventories, immutable generations, forward recovery,
archive-backed retirement, hard I/O/workflow quotas, canonical adoption and
installed/downstream/numerical qualification remain open.

Evidence: data/experiments/lifecycle-validation/20261007-cache-copy-integrity.
