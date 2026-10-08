# Focused cleanup progress

2026-10-07: focused goal active. Archive and packaging adoption remain open;
cleanup/build and launcher configuration source repairs are adopted.

## Canonical cleanup foundation

Canonical base: `3fa1ad5f6fb2384a7626e83c5e22fa7821cb7ed5`.
Main Edit source base: `dd55d7c0f4bbf6f7f4e614e5ee71ded7982b1962`.
The twelve Main Edit commits and its unrelated dirty work were not adopted.

Canonical `make clean` now calls the guarded inventory implementation rather
than recursively deleting the build root and selected executables without
classification. `make clean-plan` inspects without deleting. Both paths bypass
compiler/library setup. Mixed cleanup and build goals are refused.

Canonical build writers now publish staged compiler outputs and exact ownership
receipts. The new default profile is `build/profiles/local-owned`; old build
trees and root binaries remain untouched. Build configuration identity and
cooperative ownership prevent configuration reuse and overlapping cleanup.
Unknown or changed outputs are refused. Do not bypass a refusal or manually
remove the build root.

The canonical-compatible staging copy passed 52 cleanup, bounded-inventory,
JSON-admission and reservation methods. After adoption, the actual canonical
`make test-top-level-cleanup PKG_CONFIG=false CLANG=false` passed its 18 methods.
No live cleanup or package operation ran. `git diff --check` passes. No commits,
installed changes or independent archive transfer occurred.

Exact adopted-file hashes, prior bytes, dependency discovery and staged test
output are under
`data/experiments/focused-cleanup/20261007-canonical-clean-foundation/`.
That directory is retained evidence, with independent preservation still pending.

## Build adoption acceptance

The canonical-compatible source snapshot passed the full default-profile build,
clean-plan, clean and rebuild sequence, followed by Water, scene-project cache
output and CLI proofs. Experiment/reference/test sentinels, a legacy build note
and the older application binary remained hash-identical throughout. A separate
Clang application source build passed without launching a GUI. Twenty-six build
helper methods passed; the actual canonical control Make target passed its
18 cleanup methods after adoption. The earliest staged root-output and dependency
mapping refusals, plus the old CLI-path failure, remain in the evidence record.

Thirty-six reviewed Make/helper/test/path/documentation files were adopted with
before/after hashes and preserved prior bytes under
`data/experiments/focused-cleanup/20261007-canonical-owned-build/`. No C source,
shared subtree, version, commit, installation or release changes were adopted.
The supported headless build also passed directly in canonical's new profile.
Its log is `canonical-headless-build.log` in that record. Canonical read-only
clean inspection passed with compiler/package discovery disabled, and executable
version readback passed. Root legacy binary hashes sampled during this build
were unchanged at completion. No live canonical cleanup or simulation fixture
reset was needed for these direct checks.

The concise operating contract is `docs/cleanup_operations.md`. Read-first CLI
documentation and the affected fixture binary paths now match the selected
profile. The supported fixture scripts still reset their named output directories;
their validation here used a disposable checkout. Do not run them over retained
work merely because compilation cleanup is guarded.

## Next bounded work

Archive coverage, the final operating contract reconciliation and packaging
adoption remain open under the focused
completion plan. No exhaustive audit expansion is required for those items.

## Private launcher configuration

Canonical launchers now initialize a verified private configuration candidate
and publish it only after complete inventory comparison. macOS package links
are preserved in retained attempts while migrating to private configuration.
Failed Linux copies never become the active configuration. Unmarked existing
directories are preserved and held for explicit adoption; abrupt initialization
death leaves an ownership hold. Existing marked user configuration survives reruns.

The common helper requires existing shell/SHA tools, with no Python dependency.
Both package recipes copy it into their resources. Thirty-seven source fixture
methods pass through the control-only Make target, including native macOS tools,
stub app writes, failed/partial copies, legacy migration/adoption, publication
failure, abrupt death, sparse size admission and actual helper copy recipes.
No live package build, installed profile, GUI or remote operation was used.
See `launcher_configuration_recovery.md` for operator instructions and limits.
