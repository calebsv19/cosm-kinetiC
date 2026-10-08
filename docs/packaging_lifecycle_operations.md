# Local packaging lifecycle operations

Canonical source, uncommitted, 2026-10-07. This guide records the adopted local
mechanisms. It does not authorize a package build, signing, installation,
publication, Registry change or remote worker operation. Those actions still
follow the workspace Release Control route and their specific authority.

## Package preservation

Existing package and release outputs are preserved. `release-clean`, Linux
worker/desktop package cleanup and desktop package removal now inspect guarded
namespaces and refuse existing artifacts rather than recursively deleting them.
Do not use a changed root or a manual recursive deletion to bypass a refusal.
Use an explicitly selected fresh staging root for new work; retained artifacts
need a separate reviewed retirement decision.

Linux worker and desktop assembly use retained transaction attempts, captured
inputs and tools, mapped private staging outputs and verified publication/reuse.
Proof and determinism lanes retain fresh capsules and comparison outputs instead
of resetting old proof directories or replacing the first archive. A successful
source helper test is separate from a successfully built platform package.

macOS assembly requires a fresh reserved package destination. Dependency bundling
takes cleanup/per-app ownership, snapshots the selected binary and Frameworks,
and retains command diagnostics and terminal receipts under
`data/experiments/package-bundler-attempts/`. Dependency inspection, selected
copy/rewrite and unresolved dependency failures stop the operation. A failure
can leave partially changed staging output: preserve its snapshot and failed
attempt, and hold the complete package. Automatic rollback and complete Mach-O
dependency closure are not claimed.

Desktop replacement preserves admitted predecessors through the existing helper;
canonical and Main Edit refresh authority checks remain separate. No refresh was
run during this source adoption. Main Edit source itself has not been synchronized
with the newly adopted canonical changes.

## Optional Linux desktop entry installation

The package includes `share/install-desktop-entry.sh` and its sibling standalone
Python helper. Python 3 is required for this optional installer. Installation
does not start the GUI and needs no source-checkout imports.

```sh
share/install-desktop-entry.sh --plan
share/install-desktop-entry.sh
share/install-desktop-entry.sh --recover ATTEMPT_ID
```

Plan is read-only and reports the selected user-data paths and pending attempts.
Ordinary installation is held when an unfinished attempt exists. Recover selects
one exact retained attempt; inspect its history and use its actual ID.

The installer retains predecessor bytes/modes, publishes a content-addressed icon
before atomically replacing the desktop entry, and keeps the entry as the single
commit point. Interrupted installation leaves an entry referencing an available
icon. Recovery checks package/control identities and current output state; it
refuses to overwrite a changed user entry. It does not prune old icons or attempts.
History lives under `<XDG_DATA_HOME>/PhysicsSim/desktop-entry-installs/`, defaulting
to `$HOME/.local/share/PhysicsSim/desktop-entry-installs/`.

## Verified scope and limits

The integrated source passed 181 distinct affected methods: installer (33),
bundler (24), package outputs (8), transaction (27), proof (14), retained commands
(8), desktop replacement (14), process-audit admission (5), reservations (11),
and launchers (37). Tests use disposable paths and substitute applications or
selected tools where stated in their fixtures.

A separate native macOS fixture compiled a tiny unsigned Mach-O application and
dylib, bundled the dylib with the actual engine, and read back executable and
dylib dependency identities using `otool`. It did not sign or execute the result.
Its first fixture assertion expected the wrong dylib ID; that failed check is
retained alongside the corrected passing readback. Earlier regression failures
exposed remaining broad cleanup and missing worker transaction integration; the
adopted repairs passed their affected suites afterward.

No real Linux desktop session or `desktop-file-validate` was available. Full GUI
package builds, signing, installed-app acceptance, power-loss recovery, complete
external-descendant verification, hostile path swaps, aggregate storage quotas
and historical retirement remain separate boundaries. None is reported as
proven by these source/fixture results.

Adoption receipts, prior source bytes, native fixture artifacts and test output
are retained under `data/experiments/focused-cleanup/`. Independent archive
coverage of the frozen repairs is verified by transfer and independent retrieval;
see `focused_cleanup_closeout.md` for exact coverage and exclusions.
See `cleanup_operations.md` and `launcher_configuration_recovery.md`
for the ordinary build and private runtime configuration contracts.
