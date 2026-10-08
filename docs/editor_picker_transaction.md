# Editor picker and drop transactions

Main Edit's editor now uses one admitted add transaction for picker double-click,
Enter and a new canvas drop. Failed conversion/loading no longer appends an
unresolved ImportedShape, dirties/selects the scene or closes the picker. A full
scene or missing/inconsistent library refuses before conversion. Picker count/row,
empty or nonterminated path and model path capacity are checked. Drop positions
must be finite. The drag-release duplicate add implementation was removed in favor
of scene_editor_input_drop_import_from_picker; it uses the same candidate/commit
path and establishes the drop position before selection callbacks.

The transaction loads a candidate even when an in-memory name match exists. The
candidate must independently resolve from its persisted path under the existing
shape_lookup rules, avoiding a newly added unresolved identity. It then replaces
a matching library slot in place, preserving prior scene index references, or
appends only after allocation succeeds. Candidate failures release their storage
and preserve the previous library/working scene. App-owned picker library growth
is bounded to 1,024 slots; a matching slot may be refreshed at the bound. Existing
lookup remains name/stem based, not authenticated path identity or collision-free
provenance. Different paths with equal lookup names still share a slot.

Dropping an already present exact path retains the previous selection-only
behavior: no duplicate, conversion, asset reload or dirty change. Explicit picker
add/reimport performs the refresh. A successful add initializes the model, selects
it and marks dirty, then closes the picker and refreshes source listings only if
conversion occurred. The selected path is copied before callbacks can alter the
listing. File conversion/publication can succeed before later candidate load or
library allocation refuses; such generated files remain retained. This is a
working-scene/library transaction, not an atomic disk-plus-scene transaction.

Countertests link the original picker object from the retained preceding profile
with real native conversion, loading and lookup. Nine of the original eleven
methods fail: missing/malformed/cached-invalid candidates, stale reimport geometry,
conversion failure, full scene, missing library and identity mismatch. The other
two normal success cases pass. These tests use disposable roots and UI-effect
stubs, not the installed application's interaction loop.

Final validation passes 56 distinct methods: 24 picker/drop, 17 actual conversion
publication and 15 CLI publication. The native harness uses the real SceneEditorState,
FluidScenePreset, shape library, conversion/reader and lookup; selection, dirty and
listing callbacks are small observation stubs. The actual picker source is compiled
with one realloc fault seam for a controlled growth failure; the normal production
translation unit is also compiled by Make. Failed-state tests compare the entire
working preset byte-for-byte and inspect cache/count, selection, dirty, picker and
refresh effects. Successful cases prove stable cache IDs/refreshed coordinates,
existing scene reference, finite drop position and selection without duplication.
No full GUI/link/installed or human-interaction acceptance is claimed.

The fresh editor-picker-transaction-20261007 profile builds both shape tools and
all three affected editor translation units. Normal PGM/asset bytes match the
unchanged retained baseline; source remains unchanged. Matching repeat preserves
19 object/binary/manifest outputs. Read-only clean preview admits 36 owned files;
no cleanup applies. Current builds/tests/proof sessions are terminal. Older raster
sanitizer workers remain unresolved according to the prior retained process
snapshot; this slice makes no new termination/reaping claim.

The legacy shared shape_asset_load_file still opens blocking and sizes/allocates
without the host strict bounded file contract. The transaction fix does not make
that reader safe against FIFO/symlink/oversize/schema abuse. A borrowed-text decode
seam plus existing host admission is the next related input migration. Errors are
currently stderr/false with the picker left open; dedicated native UI diagnostics
remain an acceptance gap. Complete file/scene recovery, hard quotas, terminal-owner
retirement, upstream/canonical adoption and independent newer backup coverage
remain open. This packet is outside the frozen seventy-packet snapshot. No commit,
canonical mutation, install, release, remote transfer or user-evidence deletion
occurred. The full TL01-TL13 goal remains incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-editor-picker-transaction.
