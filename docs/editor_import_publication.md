# Editor import conversion publication

PhysicsSim Main Edit editor import conversion now uses the same app-owned
physics_sim_shape_asset_publish helper as shape_asset_tool. The helper was
extracted unchanged from the qualified CLI writer into import/shape_asset_output;
it reuses the shared shape text serializer and PhysicsSim retained persistence.
It rejects current input/output inode aliases, invalid/oversize names and output,
unsafe destinations and cooperative contention. A failed pre-publication write
retains the prior destination and failed candidate. No JSON schema or persistence
implementation was duplicated. This is app policy reuse, with no additional
shared API/version change in this slice.

The editor conversion function is separated into scene_editor_import_conversion.c
for native testing without the UI loop. Source discovery declares it in the app
graph; it is explicitly excluded from the headless worker, like the original
editor helper. SHAPE_OUTPUT_OBJS declares the common output helper, so both tools
inherit ordinary object/dependency identity and content-forced link propagation.
The native build compiles both affected editor translation units and both tools.
It does not qualify a final GUI executable or installed UI interaction.

Default conversion output is data/runtime/<stem>.asset.json, created through the
existing admitted runtime-directory helper. SHAPE_ASSET_DIR may select an admitted
existing external parent; protected Git source roots refuse. configured_root
continues to select input/library discovery in surrounding editor code, but no
longer selects mutable conversion output. The runtime asset library's source-load
roots remain unchanged. Conversion regenerates from the selected input rather
than trusting an existing filename as a valid/current cache. Successful repeated
imports replace the generated file. Equal stems share that output name; this is
not immutable identity/history or collision-free evidence storage.

The name, temporary output buffer, caller return buffer and 256-byte ImportedShape
path capacity are checked before conversion/write. Truncation refuses rather than
publishing an asset that cannot be represented by the model. Empty documents are
freed; allocation failure for the replacement asset name refuses. Failure leaves
an empty return string when capacity is positive. The shared output bound is
64 MiB after serialization, not a hard allocation quota or I/O deadline.

Countertests compile the exact old conversion/basename functions extracted before
editing, with the retained native shape objects and original data_paths source.
In disposable fixtures the old function accepted malformed existing asset bytes,
wrote into source config/objects while returning a truncated eight-byte path,
and blocked on FIFO until subprocess timeout killed/reaped the direct child.
These are function-level native countertests, not historical GUI acceptance.

Final verification passes 64 distinct methods: 17 editor publication, 15 CLI
publication, 11 mask publication, 15 persistence and six actual Make/link methods.
Editor cases cover generated defaults/source preservation, malformed cache,
invalid input despite existing cache, return/model limits, runtime creation/links,
symlink/hardlink/FIFO/directory outputs, contention, file-size fault retaining the
predecessor and 64-byte stage, and external/missing/protected parents. A first
invalid-UTF8 filename fixture failed during setup because macOS refused the name;
it is retained and excluded. CLI UTF8-name tests pass. The first Make fixture lacked
the newly declared synthetic helper source and had four failures; the corrected
six-method fixture includes preserved-time header changes for the new helper and
proves both dependent links update then become no-ops. Failed logs remain retained.

Final native profile editor-import-publication-20261007 builds both tools and
editor units. Ordinary mask and asset bytes match unchanged retained baseline;
source is preserved. Matching repeat preserves 18 object/binary/manifest files.
Read-only clean preview admits 35 owned files; no cleanup applied. All current
build/test/proof sessions are terminal. The older raster sanitizer worker pairs
10227/10253 and 11883/11915 were read-only reobserved; the children remain UE.
No restart, termination or reaping is claimed for those holds.

Open boundaries include the editor picker's failure-to-add behavior and in-memory
asset cache freshness, direct shared asset readers, generation-consistent reads,
full interruption/durability recovery, hard workflow quotas and retirement.
The converter repair does not prove those surrounding UI paths. Legacy shared
shape_asset_save_file still directly truncates, but no PhysicsSim src caller
remains after this migration. Canonical adoption and newer independent backup
coverage remain open. This packet is outside the frozen seventy-packet snapshot.
No commit, canonical mutation, install, release, remote transfer or user-evidence
deletion occurred. The full TL01-TL13 goal remains incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-editor-import-publication.
