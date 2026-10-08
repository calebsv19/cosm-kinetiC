# Native preset and preference persistence

Main Edit preset-library and theme/font saves now stage complete files through
PhysicsSim's existing admitted descriptor-based sidecar publication. An app-local
`physics_sim_persistence` adapter holds a nofollow, regular, single-link empty
kernel lock for the admitted parent namespace. Competing cooperative saves fail
before staging. Current parent, predecessor and lock witnesses are checked before
publication. Linked/special/hardlinked destinations, replaced parents/locks,
protected source roots and missing parents hold.

The existing destination is not opened for truncation. A sibling pending file is
flushed, checked, synced and closed before atomic replacement. Failed candidates
remain retained; they are not automatically replayed or pruned. On successful
replacement the directory is synced and the published identity checked. A failure
after rename returns false/unconfirmed; it can leave complete replacement bytes
visible. Callers must inspect actual state rather than assume false means that
no publication occurred. This is cooperative local exclusion with identity
checks, not adversarial atomic compare-and-swap or full path-swap confinement.
A noncooperating writer can still race the final check/rename window.

Theme/font setup admits the runtime path before creating `data/runtime` with
nofollow directory descriptors and checked sync. It never traverses linked data
or runtime directories. Preset saves require their parent already to exist, as
before. Preset slot-count/capacity/null-slot and fixed object-array bounds and
serialized fixed-string termination are checked before allocating a save attempt.
Existing valid preset serialization/version and preference values are unchanged.

The adapter admits predecessor files and completed stages up to 16 MiB. This is
a publication bound, not a streaming disk quota or hard I/O deadline. A serializer
can write a larger pending file before finish holds it. Full aggregate storage
budgets, authenticated process identity, save-result UX, exact attempt recovery
and retirement remain open. Main's existing preset-save callsites ignore bool
results; a held save is data-preserving but not yet surfaced to that GUI user.

## Shared reuse decision

`core_io` owns generic I/O, but its current atomic whole-file helper does not
supply fsync/directory sync, admitted predecessor policy, cooperative ownership,
retained failure evidence or a stream serializer contract. `core_data` and
`core_pack` supply data/format semantics rather than this application lifecycle.
Shared extension is deferred until the pilot contract is qualified. This slice
reuses PhysicsSim's existing sidecar mechanism instead of changing a shared API.
`core_theme`/`core_font` continue to own preference meanings; persistence policy
is app-local. No shared versions or adoption minima changed. Two affected UI
contract source lists now include the existing `kit_render` validation source
required by their current dependency, without changing shared code or version.

## Verification

Fourteen focused compiled-native methods pass: create/replace, same-process and
real-process competition, killed-owner retention/release, four prepublication
fault classes, postrename sync uncertainty, linked/special/changed destinations,
changed lock/parent, runtime admission and actual theme/font save behavior.
Existing sidecar atomic (six) and job metadata atomic (eleven) regressions pass.
Four actual Make contract entrypoints pass with explicit
`BUILD_DIR=build/profiles/native-persistence-20261007`: preset dimensional
roundtrips plus invalid-input predecessor preservation, theme/font resolution,
UI button and workspace authoring host. The latter has silent successful exit.

The first contract invocation accidentally used unsupported `BUILD_PROFILE` and
therefore ran in default Main Edit build storage; it passed and did not clean.
The explicit isolated rerun is authoritative. Failed preference tests used an
unsupported font name and were corrected against the actual core_font catalog.
A consumer link failure exposed the missing existing validation dependency and
was repaired. Failed and passing logs are retained. Tests use disposable data;
no actual user's preference/preset files were modified by fault injection.

This advances TL02/TL03/TL07 for three native save entrypoints. It does not close
the broader lifecycle goal. General text and binary snapshot writers and coherent
scene-cache replacement remain untouched. No canonical adoption, commit, cleanup,
pruning, install, package or release occurred. This evidence is local and outside
the frozen independently archived snapshot and later prepared backup batch.

Evidence packet: `data/experiments/lifecycle-validation/20261007-native-persistence`.
