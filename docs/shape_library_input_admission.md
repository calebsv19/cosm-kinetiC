# Startup shape library admission

PhysicsSim Main Edit main now initializes an empty library and calls the app-owned
physics_sim_shape_library_load instead of shared legacy directory loading. Failure
reports unavailable/refused storage and continues with an empty library; it no
longer silently accepts a partial selected library. No legacy asset file/directory
loader calls remain under PhysicsSim src. The shared legacy APIs remain available
for compatibility outside this app migration.

The new loader reuses a small directory-open seam in physics_sim_job_json: the
existing ancestor/protected-component and known macOS alias admission, nofollow/
nonblocking directory open and pre/post named/descriptor identity checks. File
reading still delegates to physics_sim_shape_asset_load and its strict typed/
bounded JSON contract. No new path parser or shape decoder was introduced; this
extends app-owned purpose/metadata I/O, not a shared module API or version.

It bounds enumeration to 4,096 entries excluding dot/dot-dot but including ignored
names, at most 1,024 selected nonhidden lowercase .json files, 64 MiB aggregate
file bytes, 100,000 decoded points and 10,000 decoded paths. The per-file 16 MiB /
1,024-path / 10,000-point contract remains. The complete regular/single-link byte
inventory is checked before any file is decoded. Aggregate geometry is checked
as each independently bounded file returns; this is not a hard heap/CPU/I/O quota.
All selected failures, including allocation/enumeration errors, hold the whole
candidate. Empty/missing directories return false. The caller must provide empty
output storage; nonempty input is refused without changing it.

Assets retain bytewise sorted filename order, defining deterministic startup slot
indices. Operational asset names must be nonempty and unique. Duplicate detection
sorts separate inventory metadata by identity, retaining asset slot order and
avoiding quadratic name comparison. Names still follow the existing app name/stem
lookup model; this is not path-authenticated or collision-free cross-library
provenance. Eight checked-in assets satisfy the stronger operational contract.

The candidate is not published until every selected file witness and directory
witness is rechecked, the original path is re-admitted/reopened as the same
directory, and enumeration closes successfully. File identity, size, links, mode,
mtime and ctime changes refuse; ignored directory additions/removals conservatively
hold the inventory too. This is a trusted-local observed generation, not a lock,
immutable snapshot, hostile ancestor-race sandbox or producer authentication.
Changes after return are outside the observed startup interval.

Native legacy countertests run eight directly comparable cases against the old
shared directory loader compiled from its unchanged source. Seven fail and one
FIFO times out, with the direct child killed/reaped by subprocess timeout. Evidence
covers partial malformed libraries, linked files/directories, duplicate/unnamed
identities and replacement of a nonempty caller library. New fault-only tests are
excluded from legacy counterclaims. Deterministic ordering is source/new-behavior
proof, not an assertion that every historical filesystem enumerated differently.

Final validation passes 47 distinct methods: 34 actual library, nine existing
strict JSON reader and four metadata observation. Native directory tests use real
file/typed asset readers; one source-only fault build controls calloc, readdir and
post-read file/directory changes. Normal production loader and actual main caller
also compile through Make. Tests include limits and accepted exact entry/asset/
aggregate-geometry boundaries, malformed mixed libraries, FIFO/link/special files,
protected/alias paths, allocation/enumeration failure, content mutation, replacement
and directory additions/swaps. All eight checked-in geometries compare byte-for-
byte after serialization by asset identity against legacy decoding, with original
file hashes unchanged. No full GUI link, installed/native interaction or public
release acceptance is claimed.

The fresh library-input-admission-20261007 profile builds both shape tools, main
and the new loader. Normal PGM/asset conversion bytes match unchanged retained
baseline; source remains unchanged. Matching repeat preserves 19 object/binary/
manifest outputs. Read-only clean preview admits 36 owned files; no cleanup applies.
All current builds/tests/proof sessions are terminal. Older sanitizer workers are
not restarted or claimed reaped. Canonical remains clean and unchanged.

Source/path identity mapping across saved scene generations, full desktop/installed
qualification, UI diagnostics for held startup, downstream raster/transform safety,
hard global quotas, interruption recovery and explicit retirement remain open.
Canonical adoption and independent newer backup coverage remain pending. This
packet lies outside the frozen seventy-packet snapshot. No commit, canonical or
shared mutation, version change, install, release, remote transfer or user-evidence
deletion occurred. The full TL01-TL13 lifecycle goal remains incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-library-input-admission.
