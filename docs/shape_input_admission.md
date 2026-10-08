# Shape input admission and vendored text decoder

PhysicsSim Main Edit shape_import_load now reuses the existing app-native strict
JSON reader: nofollow/nonblocking open, regular single-link input, 16 MiB bound,
read-size and whole-file identity checks, ancestor/path admission with known macOS
system aliases, then strict JSON object admission. The existing contract bounds
depth to 64 and structural values to 100,000 and rejects embedded NUL, trailing
data, duplicate decoded keys, invalid UTF-8 and nonfinite numeric tokens. Source
files in the repository remain supported. This is trusted-local input admission,
not a hard I/O deadline, arbitrary-upload sandbox or hostile ancestor-swap proof.

An additive ShapeDocument_LoadFromJsonText seam in Main Edit's tracked vendored
shape snapshot accepts borrowed NUL-terminated text and reuses the existing shape
decoder. The legacy file API wraps that seam and keeps its prior behavior. The
PhysicsSim host passes admitted JSON through json-c serialization into the seam;
it never reopens the untrusted source filename after admission. The decoder is
not itself a strict parser or bounded file reader. No second shape decoder or
app-local duplicated JSON grammar was introduced.

App policy requires exact consumed key spelling, optional integer version 1,
arrays/objects and recognized line/cubic types, optional correctly typed names
and closed flags, and two finite float-representable coordinate numbers. Consumed
case aliases refuse because the legacy cJSON field lookup is case insensitive;
otherwise an unvalidated case variant could win decoding. Unknown metadata fields
remain supported under the strict whole-JSON contract. Whole-document limits of
1,024 shapes, 10,000 paths and 10,000 segments apply before the shared indexed
linked-list decoder loops/allocations. Loaded nonempty-path shapes also pass the
existing conservative geometry preflight at the shared minimum spacing 0.01,
protecting both mask and asset flattening from the pre-clamp integer conversion
and excessive logical geometry. Empty shape/path compatibility remains. Failed
load frees its candidate and leaves an empty output document, as the prior loader
contract expected; callers must provide empty output storage.

The input reader uses its existing ordinary owned object and JSON library flags
in both shape final links, including dirty-object propagation and Darwin link
provenance. A real Make fixture proves preserved-time reader-header changes
rebuild both links and then become a no-op. Ordinary app graphs already declare
this object; this does not establish complete worker/platform qualification.

Reuse decision: reuse-extended for the unversioned vendored non-core shape decoder;
reuse-adopted for existing PhysicsSim strict JSON admission. core_io read-all lacks
this bounded/strict admission contract; core_data/core_scene do not own the legacy
shape wire decoder; presentation kits have no role. Path/schema/resource policy
stays app-local. Canonical shared shape source/header remain checksum-unchanged.
No shared/core or shared/kit API/version changed, no shape VERSION was invented,
and no ecosystem minimum/promotion is claimed. This is a local development
candidate that must be preserved/reconciled during later managed vendor/upstream
adoption; it is not an accepted cross-program shared release. Local vendored
README and compatibility/current-state/gap notes record that boundary.

Final validation passes 65 distinct methods: 12 actual input, 6 decoder compatibility,
9 strict reader, 11 PGM publication, 7 CLI numeric, 5 actual Make/link and 15 raster
sanitizer methods. Legacy file versus text decode outputs match for line, cubic,
Unicode, empty document/shape, unknown legacy type fallback and invalid version.
Host tests include linked paths/ancestors, hardlinks, FIFO, oversize input, trailing
and embedded bytes, duplicate/case keys at every level, semantic type/coordinate
failures, UTF-8/depth and geometry/whole-document bounds. The old native gate has
28 failed subcases and three errors: two FIFO timeouts with direct children
killed/reaped and one Python UnicodeDecodeError after invalid UTF-8 reached old
mask stdout. Some failures show wrong-stage diagnostics rather than successful
malformed input. No complete old-source sanitizer gate is claimed.

The terminal final fresh shape-input-admission-20261007 build produces both tools.
Five normal line/curve/transform mask cases and normal asset output match unchanged
retained baseline bytes; source remains unchanged. Matching repeat preserves 15
object/binary/manifest outputs. Read-only clean preview admits 28 files. The first
JSON regression discovery used a nonexistent filename and returned exit 5 with
zero tests; it is retained but excluded. The corrected nine-method reader gate
passes. No cleanup or old-profile rebuild occurred.

Two earlier sanitizer attempts were reobserved in UE state under their original
waiters (10253/10227 and 11915/11883); no restart, termination or reaping is claimed.
Current final build/test/proof sessions are terminal and separate from those holds.
Remaining work includes asset output publication, direct legacy file-loader
consumers, hard I/O/storage/workflow limits, complete reader generations/recovery,
retirement, upstream/canonical adoption and independent newer-backup coverage.
Installed/public/platform/physical-CFD acceptance remains separate. No commit,
canonical shared/PhysicsSim source mutation, version change, install, release,
user evidence deletion or independent archive transfer occurred. This packet lies
outside the frozen seventy-packet backup. The full TL01-TL13 goal is incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-shape-input-admission.
