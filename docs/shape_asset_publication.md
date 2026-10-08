# Shape asset JSON publication

PhysicsSim Main Edit shape_asset_tool now serializes through the existing shared
shape schema and publishes through PhysicsSim's retained persistence helper.
It no longer opens the destination with direct truncation. Checked writes and
finish/abort preserve the predecessor on pre-publication failure; failure exits 1.
Current input/output inode aliases refuse. The helper admits existing parents,
regular unique destinations and generated Git roots, refusing linked ancestors,
symlinks, hardlinks, special files, source storage and cooperative lock conflicts.
Failed candidates remain retained. A failure after rename can mean complete
visible bytes with unconfirmed durability; full path-swap confinement, durable
recovery inventories and immutable evidence history are not claimed.

The converter's default moves from config/objects to data/runtime/<stem>.asset.json.
Only this generated default creates its admitted runtime directory. Explicit --out
and SHAPE_ASSET_DIR destinations require an admitted existing parent. The runtime
asset library still loads config/objects or SHAPE_ASSET_DIR; generated output is
not automatically promoted into the source asset library. Asset names are bounded
to 1 MiB and verified through the existing strict JSON parser. Serialized output
is limited to 64 MiB after construction; this is not a hard serializer allocation
quota. Existing geometry/input admission applies before conversion.

The local unversioned vendored non-core shape module adds
shape_asset_to_json_text and shape_asset_json_text_free. The latter uses cJSON_free
so custom allocator ownership is respected. All reached cJSON construction/print
allocations are checked, invalid pointer/count pairs refuse, and nonfinite points
refuse. The existing path API reuses this serializer and checks fclose, but still
truncates its destination directly. A real file-size fault demonstrates that it
returns false while leaving a partial file; legacy callers require later migration.
The host reuses ordinary owned persistence/headless-output objects. Both shape
links propagate content-forced helper rebuilds through SHAPE_OUTPUT_OBJS.

Reuse decision: extend the existing shared schema serializer, adopt the existing
PhysicsSim persistence helper, keep host path/resource policy app-local. No
second JSON wire serializer or new shared abstraction was added. Canonical shared
source/header checksums remain unchanged. This is a local development vendor
candidate, not an accepted upstream release or ecosystem minimum. No module
version was invented. Preserve/reconcile the candidate during managed adoption.

Final validation records 75 distinct methods: 15 actual asset publication, four
serializer, 12 input, 11 PGM, seven numeric, five Make/link, 15 persistence and six
sidecar. Serializer tests fail each of the 25 reached allocation boundaries and
verify release without tracked leaks; normal text/file bytes match. Publication
covers missing/linked parents, symlink/hardlink/FIFO/directory, contention, source
alias/storage, UTF-8, default/external destinations and create/replace. RLIMIT_FSIZE
with SIGXFSZ ignored leaves the old file intact and a retained 64-byte candidate.
Old countertests have seven failures and one FIFO timeout across eleven methods;
the direct FIFO child was killed/reaped. Initial harness compilation lacked the
include path and its discovery could not run; those failed logs are retained and
excluded from successful counts. Final build and test sessions are terminal.

The fresh shape-asset-publication-20261007 profile builds both tools. Ordinary
32x32 mask and asset output match the unchanged retained baseline byte-for-byte;
source is preserved. Matching repeat preserves 15 object/binary/manifest outputs.
Read-only clean preview admits 29 files; no cleanup applied. Earlier unrelated
sanitizer workers remain unresolved in the retained process snapshot; these
successful tests do not prove those workers terminated.

Remaining work includes legacy direct-file consumers, complete recovery/readers,
hard workflow quotas, terminal-owner retirement, upstream/canonical adoption and
independent backup coverage for newer packets. No commit, canonical mutation,
version change, install, release, user-evidence deletion or archive transfer is
claimed. The full TL01-TL13 goal remains incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-shape-asset-publication.
