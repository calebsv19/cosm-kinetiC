# Startup runtime directory lifecycle

Main Edit `physics_sim_ensure_runtime_dirs` now uses a fixed directory graph:
`data`, `data/runtime`, `data/runtime/scenes`, `data/snapshots`. All existing slots
are inspected with nofollow metadata and directory descriptors before creating
any missing slot. A link, ordinary file, FIFO or other invalid directory slot
holds the entire initially observed plan. For example, an existing linked
snapshot slot refuses setup before allocating a missing runtime directory.

Missing slots are created relative to pinned directory descriptors, with private
0700 requested permissions for new directories. Existing directory permissions,
inodes and contents are preserved. Root and opened-slot witnesses are checked
between steps and at completion; children and their parents are synced. Changed
parents/slots, failed mkdir/open or sync return false. Partial new directories
are retained and later admitted setup can reuse them; there is no rollback,
recursive deletion, chmod of existing storage or pruning.

The root is current cwd. System/protected roots, the resolved home root and
subdirectories beneath a Git checkout marker are refused; running from the
checkout root or a standalone non-checkout working directory is supported.
Unreadable marker checks and unresolvable HOME hold. Home comparison resolves
aliases such as macOS /var and /private/var. This fixed app-policy helper is not
a public arbitrary-path storage API. Preference-only setup still has its own
previously qualified runtime adapter; it does not imply whole-graph setup.

This remains cooperative/local filesystem hardening, not complete hostile
path-swap confinement. A noncooperating swap during a syscall window can leave
creation in a pinned former parent; tested swaps leave the linked external
target untouched and report held. Directory creation is monotonic and has no
whole-operation writer lock. No hard I/O deadline, aggregate storage quota or
GUI/runtime acceptance is established here. Descriptor closure is explicit on
all ordinary return paths.

Ten actual compiled native methods pass: fresh setup and inode/content-preserving
reuse; linked data/runtime refusal; late invalid snapshot refusal before runtime
allocation; invalid scene refusal before snapshot allocation; regular/FIFO data
refusal without blocking; partial creation retention/reuse; sync-failure hold;
controlled parent swap; checkout-root/source-subdirectory admission; home/system
root refusal. Existing six configuration-save and fourteen native-persistence
methods also pass. Actual isolated configuration and workspace-authoring Make
consumer contracts pass in `build/profiles/runtime-dirs-20261007`. The workspace
host test exits successfully without a success banner. Existing public caller
signatures and dependency source lists remain unchanged; all helper operations
are implemented in the existing data_paths translation unit.

Initial test-harness compilation lacked the rename declaration and was corrected.
The home-root test then exposed unresolved path aliases in the implementation;
normalizing HOME repaired that actual admission gap. Both failure logs and final
passing logs are retained. The editor and menu duplication callers already turn
a false setup result into failure diagnostics; startup already logs failure.
No installed app or actual user-data fault injection was performed.

Reuse classification: app-local fixed graph/path policy, using the existing POSIX
descriptor admission pattern. core_io's generic whole-file atomic API and
core_data/core_pack data/format contracts do not own this program-specific
startup graph. Shared extension remains deferred pending pilot qualification;
no shared API/version, minimum or adoption state changed.

This advances TL02 for startup setup. Configuration parsing/escaping, general
text/snapshot saves, coherent scene-cache publication, full worker lifetime,
resource budgets, archive-backed retirement and canonical adoption remain open.
No commit, cleanup, pruning, package, installation or release occurred. Evidence
is local and outside the original independent archive and later frozen batch.

Evidence: `data/experiments/lifecycle-validation/20261007-runtime-directory-lifecycle`.
