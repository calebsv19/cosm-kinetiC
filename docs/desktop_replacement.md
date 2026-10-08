# Desktop app replacement preservation

This is a source/Main Edit safety contract, not installed-product acceptance or
release authority. Existing canonical and Main Edit refresh gates still apply.
Release Desktop refresh now also requires the canonical refresh authority gate.
Its existing one-worktree condition remains unresolved; this change does not
weaken it. No real package, signing or installation was performed for this slice.

The Make refresh recipes call `scripts/desktop_replace.py` after their existing
build/proof and ownership gates. Sources must be real PhysicsSim app directories
under checkout build/dist; destinations must be the matching `kinetiC.app` or
`kinetiC Main Edit.app` directly under the current user's Desktop. Other output
locations require a separately designed preservation contract. Source and
predecessor Info.plist bundle IDs must match the declared identity. External
bundle links and special files are refused before replacement.

Each invocation holds a cooperative destination lock and creates a fresh attempt
under `~/Desktop/.physics-sim-app-history/`. The receipt records source and
predecessor inventories, including file SHA-256/size/mode, internal link targets
and empty directories. macOS copies use ditto to preserve bundle attributes;
these inventories do not independently prove resource forks, xattrs, signatures,
notarization or application behavior. Other platforms use copytree for fixtures.

The candidate is staged and verified, then any installed predecessor is copied
and verified. The original installed directory is moved into `displaced/` and
the candidate is moved to the Desktop name. Nothing is recursively deleted.
Completed attempts retain both predecessor copies and the receipt. A caught
publication failure restores the original name when its inventory still matches.
Source drift, copy mismatch or unknown installed identity stops before replacement.

The two renames are not one atomic swap. SIGKILL or machine failure in that
interval may leave the Desktop name absent. Unfinished or unreadable history
holds subsequent refreshes. Before recovery, inspect the exact attempt receipt
and inventory of `predecessor/` and `displaced/`, confirm process ownership and
whether the Desktop name is absent or occupied, and restore only the selected
verified predecessor to an absent destination. Do not merge into an occupied app
or edit a receipt to bypass the hold. A typed recovery command and local SIGKILL boundary proofs are now implemented;
power-loss qualification remains required work; no durability claim beyond recorded local
copy/readback and catchable failure restoration is made.

History is local retained evidence, not an independent backup. Normal repo clean
has no ownership over it. Pruning requires explicit archive/readback and retained
recovery records through the existing lifecycle owner. Package removal now holds
existing staging outputs; Main Edit source-drift rejection retains the failed
package instead of erasing its diagnostic payload.

Eight disposable app tests cover repeated successful replacement, copy and
publication failures, restored predecessors, exact identity/path/link/special-file
holds, active-owner exclusion, unreadable/unfinished history, and the actual Make
refresh recipe with build/authority prerequisites stubbed in a fake HOME. These
are filesystem and recipe proofs, not real package or authority qualification.


## Exact attempt recovery

First obtain a read-only plan for the exact retained history directory:

```bash
python3 -B scripts/desktop_replace.py --recover-attempt "$HOME/Desktop/.physics-sim-app-history/<attempt-id>"
```

After inspecting its destination, action and bound receipt checksum, an operator
with authority to recover that local app can apply the same selection by adding
`--apply`. The command holds the same destination lock, revalidates the plan,
checks all available predecessor inventories and stages a verified copy before
restoring an absent Desktop name. It never merges into or overwrites an occupied
app, including an empty app directory: native exclusive rename is required and
unsupported filesystems fail closed. Darwin uses renamex_np(RENAME_EXCL); the
Linux branch uses renameat2(RENAME_NOREPLACE) but was not executed on this Mac.

If the current name already matches the journaled predecessor or candidate,
recovery records that observed state without replacing it. Unrelated contents,
tampered predecessor copies and active ownership hold recovery. Repeated recovery
with matching readback returns already_recovered. A completed recovery remains a
terminal historical event for later refresh admission; later successful versions
do not reopen it. Recovery does not certify app behavior or authentication.

The original attempt receipt remains unchanged. Separate recovery receipts under
`<attempt>/recovery/<recovery-id>/` bind its SHA-256, selected destination,
inventory and terminal result. Failed recovery staging is retained for diagnosis;
verified retries use fresh recovery identities. An absent name with no original
predecessor can be reconciled as absent, without installing a candidate.

Fourteen disposable app tests now cover real child SIGKILL before displacement,
after displacement and after candidate publication; CLI plan/application,
idempotency, subsequent refreshes, tampered/occupied/locked recovery holds,
failed recovery copying and exclusive publication to an occupied empty directory.
These are local Mac filesystem/process tests, not machine power-loss proofs.

API references: [Apple exclusive rename support](https://developer.apple.com/documentation/foundation/urlresourcevalues/volumesupportsexclusiverenaming)
and [Linux renameat2 no-replace contract](https://man7.org/linux/man-pages/man2/renameat2.2.html).
The Darwin flag/prototype were also checked against the installed Xcode SDK
`usr/include/sys/stdio.h` before the local exclusive-rename test.


## Typed Main Edit process absence gate

The Main Edit refresh recipe now validates its freshly generated MEW1 process
receipt through `scripts/check_desktop_process_audit.py` before replacement.
The validator requires schema codework_mew1_process_audit_v1, the exact match
label and resolved destination, boolean running/truncated fields, an empty list
of matches, no truncation/error and lsof_path_fallback. With an explicit path,
MEW1 takes that fallback after an empty literal ps lookup; ps_literal alone is
insufficient to establish the selected-path absence. The shared producer and
canonical one-worktree/branch/clean-source gates are unchanged.

Receipt mtime must be no more than 30 seconds old and cannot be in the future;
file identity is rechecked around the bounded no-follow JSON read. Duplicate
fields, non-finite JSON, malformed/incomplete records, special files, wrong scope,
running matches and unavailable evidence hold replacement. Status metadata uses
the same duplicate-field refusal. The old grep check could miss minified
"running":true; a disposable test reproduces that formatting gap and confirms
the actual refresh recipe cannot reach its replacement stub with such evidence.

This is a bounded point-in-time process/path observation, not a process lock or
an assurance against an app starting afterward. Saved receipts are historical;
a later refresh needs a newly generated audit. It grants no process termination,
installation, authentication or release authority. A real read-only MEW1 audit
was generated and accepted at observation time; no app was replaced. Five process
validation, eight status, nine cleanup, nine proof, twenty transaction and fourteen
Desktop preservation tests plus Main Edit contract pass for this slice.
