# Linux per-user desktop installer lifecycle

The packaged share/install-desktop-entry.sh wrapper now invokes the sibling
standalone stdlib-only install-desktop-entry.py. Package assembly copies both
files, checks helper presence in its proof lane and documents Python 3 as an
optional desktop-installer prerequisite. No source-checkout imports are needed.
The GUI launcher retains its separate runtime requirements and is not launched
by the installer. This source repair does not install an existing package or
qualify a deployed Linux desktop.

## Operator contract

- share/install-desktop-entry.sh --plan reads inputs/current exact outputs and
  reports destination paths plus unfinished attempt IDs. It creates no user data.
- share/install-desktop-entry.sh installs one per-user entry and icon generation
  under XDG_DATA_HOME, defaulting to HOME/.local/share when unset or empty.
- share/install-desktop-entry.sh --recover ID reconciles one exact retained
  32-hex attempt. An unfinished attempt blocks ordinary installation. Unknown,
  malformed, changed or incompletely prepared attempts remain held.

One kernel flock in PhysicsSim/desktop-entry-installs/owner.lock serializes
cooperating installers for the selected data root. Each invocation gets a UUID
history directory with an immutable request, desired desktop/icon bytes and any
previous desktop/legacy-icon snapshots. Requests declare retained_desktop_install_history
and automatic_removal=false. Failure/recovery diagnostics are create-only; a
completion record binds the exact request hash. Prior attempts, icons and
pending candidates are never automatically pruned.

## Coherent publication

The installer no longer changes the legacy kinetic.svg. A new icon generation
is named kinetic-<SHA256>.svg, written create-only and verified before entry
publication. Exact existing generations may be reused; differing bytes/modes
are held. applications/kinetic.desktop is the single commit point: a prepared
same-directory candidate is published atomically, after source/destination
revalidation. A missing entry uses no-replace hard-link publication; an admitted
predecessor uses atomic replacement. Existing regular predecessor bytes/mode
are retained before this commit. Files and containing directories are fsynced.

A stop before entry publication leaves the old entry referencing its old icon.
A stop afterward leaves the new entry referencing an already published icon.
Recovery checks the original package launcher, source icon and installer/wrapper
identities, exact paths, retained candidates/snapshots, and current entry/icon.
It can complete an interrupted publication or reconcile an already published
entry without replacing a user edit. An already completed recovery requires
current bytes/mode to match rather than treating historical completion as proof
of current installation. No automatic rollback or deletion is performed.

Path admission requires absolute bounded paths and refuses control characters,
symlink components, special files, package/data overlap and filesystem-root data
storage. Reads are nofollow/nonblocking regular-file reads with size/identity
checks; icon and predecessor reads are at most 2 MiB, launcher reads 4 MiB,
installer helper 1 MiB and request/completion metadata 64 KiB. History scanning
has 4,096-entry, aggregate 8 MiB metadata and sampled ten-second bounds. These
are cooperative admission limits, not disk quotas, syscall deadlines or a
sandbox against unrelated local writers.

Exec paths are quoted through both desktop-string and argument escape layers;
literal percent signs are escaped. The supported Exec path is ASCII and contains
no equals sign, consistent with the specification's executable restrictions.
Icon paths use iconstring escaping. Standards references:
https://specifications.freedesktop.org/desktop-entry/latest/exec-variables.html
https://specifications.freedesktop.org/desktop-entry/latest/value-types.html
No desktop-file-validate executable was available on the local Mac, and no real
Linux desktop parser/session was invoked; formatting tests establish selected
source output only.

## Verification and remaining boundaries

Thirty-three distinct installer methods pass, including actual packaged-wrapper
plan/help/install without source imports; default/empty-XDG roots; preserved old
bytes/modes; changed-icon generations and repeated-attempt preservation;
publication errors; competing ownership; linked/special/oversized input holds;
invalid recovery IDs; source/control/candidate/predecessor drift; duplicate and
nonfinite JSON; final-boundary user edits; unknown history; current-mode checks;
and exact current completed readback. Real abrupt child exits at five checkpoints
(prepared, before icon, after icon, before entry, after entry) are recovered from
matching retained attempts. The test executes the actual two Make copy recipes
in a minimal fixture, including a package path with spaces.

Eight package-output and twenty-seven package-transaction regressions pass:
68 distinct affected methods total. Control-only Make repeats the 33 installer
methods with compiler/pkg-config unavailable and no requested profile allocation.
Shell syntax and git diff whitespace checks pass. Selected persistent fixtures
retain first replacement, publication failure/recovery and abrupt-after-icon
before/after recovery. A safe old-script control injects cat failure only inside
a disposable fixture and shows the legacy icon already replaced and old entry
truncated. No real user profile was touched. Copied evidence keeps original
absolute fixture paths; relocated replay is not claimed.

Full filesystem power-loss behavior, hostile noncooperating path swaps, aggregate
disk quotas, automated retirement, package movement after installation, arbitrary
old completion migrations and installed Linux-session behavior remain open. A
crash while constructing an attempt before a complete request leaves an orphan
held for owner reconciliation; it is not guessed safe or automatically removed.
The complete package/GUI and release/version surfaces require separate proof.
Main Edit only: canonical remains unchanged. The archive copy remains pending
exact approval, and this evidence is outside its frozen cutoff.

Evidence: data/experiments/lifecycle-validation/20261007-linux-desktop-installer.
