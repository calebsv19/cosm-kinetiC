# Top-level entrypoint scope and legacy frame retention

The first-party writer inventory previously omitted tools/packaging and
extensionless shell entrypoints. A same-current-tree comparison with the previous
scanner confirms ten omitted files: rm_frames; both packaged launchers; the dylib
bundler; Linux desktop installer; three package validation/manifest helpers; and
the Vulkan and Wind probe tools. Tools is now a selected scan root, rm_frames is
an explicitly selected root file, and extensionless files under selected roots
use a bounded 256-byte shebang probe before source parsing. Shell and Python
interpreters are recognized; unsupported interpreters become admission gaps.
Binaries are not decoded as source or executed. Existing per-file/total/entry/depth
bounds remain. Make variable bindings now expose packaging-helper paths as static
edges, in addition to recipe references. Variable expansion is not executed or
fully resolved; lexical discovery can still miss aliases/macros/dynamic writers.
Root files remain explicitly selected, not a whole-filesystem source census.

The legacy rm_frames command formerly deleted frame_*.bmp relative to the caller's
working directory without ownership, classification, terminal or recovery checks.
It now delegates to the existing bounded read-only retention audit of its own
checkout's export/render_frames. Default and --plan preserve all output. --apply,
--force, unknown arguments and excess plan arguments refuse. --help describes the
hold. Linked output roots refuse through existing path admission. Missing output
is reported without creating directories. This intentionally removes blind frame
deletion; a complete guarded frame-retirement implementation is still required.
A zero exit from the inventory means observation completed, not deletion eligibility.

Reuse: existing retention_audit, cfd_evidence path admission and versioned retention
policy. No second cleanup engine or shared-library policy was introduced. Source
changes are limited to Main Edit; canonical's rm_frames is unchanged. The initial
Make test attempt revealed the new target was not in CONTROL_GOALS and entered
the build-owner path. That diagnostic log/profile are retained. The corrected
target is control-only; a real repeat with unavailable compiler/pkg-config and a
fresh absent profile succeeds without allocating that profile.

## Fresh verification

Twenty-two distinct tests pass: nine writer discovery, six actual frame wrapper
fixtures, and seven retention-audit regressions. The two real control-only Make
entrypoints repeat fifteen of those methods, not additional unique checks. Wrapper
fixtures copy actual helper source and policy, compare file bytes/mtime/inodes,
use a checkout containing spaces and an unrelated caller, and cover default,
preview, marked evidence, linked/absent roots, deletion arguments and help.
An old-code control removed only a disposable synthetic frame, confirming the
legacy behavior; no user frames were touched. A real Main Edit --plan from
/private/tmp reports its own absent frame root and no mutations. All sessions are
terminal. No compiler, simulation, GUI, installed app or package was launched.

The current selected scan covers 1,839 files / 14,095,488 source bytes: 1,093 Python,
691 native, 28 shell and 27 Make files. It reports 6,308 static signals across
1,105 files, 746 explicit path edges (29 Make variable bindings) and no selected
admission gaps. Counts include declarations/tests/strings and are not counts of
unsafe writers or behavior acceptance. All behavior_coverage_verified fields stay
false; deletion_authorized stays false. Inventory output and logs are retained.

## Newly visible open findings

| Surface | Inspected trigger | Required next qualification |
| --- | --- | --- |
| tools/wind_orientation_probe.py | main removes an existing output_root with shutil.rmtree unless keep_existing; keep_existing still permits report/scene replacement and launches headless --overwrite. | Admit exact input/output roots and inventory; retain prior output and failed attempts; fresh identities and whole-workflow ownership; test default, rerun, failure and interrupted recovery without private CFD data. |
| tools/packaging/macos/bundle-dylibs.sh | Uses predictable TMPDIR/physicssim_bundle_dylibs.$$ with mkdir -p, then recursive removal in its trap; mutates dependency binaries and suppresses some install_name_tool failures. | Unique owned temporary allocation, explicit failure evidence, checked transforms and parent transaction binding; no assumed cleanup authority over pre-existing directories. |
| tools/packaging/linux/install-desktop-entry.sh | Copies icon then truncates a desktop entry in user XDG data storage, with no predecessor/rollback transaction. | Source-only fixture repair and coherent icon/desktop replacement; installed-host behavior and deployment remain separate. |
| Packaged Linux/macOS launchers | Log/runtime initialization precedes --print-config; macOS can also write ICD JSON. Linux copies missing resource lanes; existing roots are trusted. | Make inspection read-only; admit runtime/log roots, preserve user state and test fallbacks/linked roots; actual installed freshness requires separate qualification. |
| tools/packaging/write_linux_worker_artifact_manifest.py | Direct --output mode writes final bytes after mkdir; --verify is separate. | Confirm all owning package transactions stage this helper safely; qualify direct-call replacement policy and preserve prior artifacts on failure. |
| tools/verify-vulkan-rollout.py | Captured diagnostic log is written directly after subprocess execution. | Retained attempt identities, bounded supervision and predecessor-preserving log/report publication. |

These are current source observations, not injected failures in user/package data.
They expand the remaining work; they are not closed by repairing rm_frames.
The full TL01-TL13 requirements, canonical adoption, coherent recovery, retirement,
full process ownership and newer independent backup coverage remain incomplete.
The pending archive copy was not retried. No commit, install, release, canonical
adoption, cleanup of user output or historical-process reaping occurred.

Evidence: data/experiments/lifecycle-validation/20261007-entrypoint-audit.


## Retained Wind orientation workflow

The newly discovered Wind probe recursive reset is replaced by fresh owned
attempts. Existing output is preserved on rerun, native overwrite is removed and
workers use existing build ownership and bounded retained supervision. Twenty-nine
distinct methods, the control-only Make repeat, native baseline, full portable
three-orientation fixture, Water and scene-cache proofs pass. The reduced two-frame
native metric failure and an actual concurrent-build refusal are retained as
separate diagnostics. Matching repeat preserves 237 outputs; clean preview admits
474 without deletion. See wind_probe_lifecycle.md. Aggregate storage/process /
recovery/retirement/canonical/archive qualification and the other packaging findings
remain open. No full lifecycle, physical accuracy or historical-reaping claim.


## macOS dependency bundler ownership and failure evidence

The bundler now requires reserved local package output, takes cleanup/per-app
ownership, snapshots the selected binary and Frameworks, and runs its captured
engine in a fresh retained attempt through existing anchored supervision.
Predictable temporary storage and recursive trap deletion are removed; otool,
install-name failures and unresolved selected dependencies now fail visibly.
Forty-five distinct affected methods pass, plus the 24-method control Make repeat
with no compiler/profile allocation. Selected persistent fixtures include the old
masked-tool-failure control. See macos_bundler_lifecycle.md. Aggregate disk limits,
complete dependency/consumer qualification, automatic rollback/coherent recovery,
retirement and canonical/installed/released adoption remain open. This slice did
not retry the pending archive, run real package/signing tools, or reap historical
process holds.


## Coherent Linux per-user desktop installation

The packaged installer now retains operational-history attempts and predecessors,
publishes immutable content-addressed icon generations, and uses the desktop entry
as its single atomic commit point. Read-only --plan and exact --recover handle
interrupted attempts while changed user/source/candidate state remains held.
Thirty-three installer methods plus eight package-output and twenty-seven package
transaction regressions pass (68 distinct methods); control-only Make repeats
the installer methods without a compiler or profile. Five abrupt process-exit
checkpoints, publication errors, real wrapper/copy recipes and an old partial-write
control are retained. See linux_desktop_installer_lifecycle.md. Python 3 is an
optional installer dependency. Actual Linux desktop/session/package deployment,
power-loss/concurrent-hostile-writer proof, aggregate quotas, retirement and
canonical adoption remain open. The pending archive was not retried.


## Read-only packaged inspection and selected startup admission

Both launchers now answer --print-config before filesystem initialization, report
requested-path scope, preflight selected runtime/log destinations and fail
configured-root errors instead of switching to shared temporary paths. macOS
creates checked fresh ICD generations while preserving existing files/overrides.
Forty-seven distinct launcher/release-audit/package-proof methods pass; control
Make repeats 25 without a compiler/profile. Native-tool and missing fixture
dependency failures are retained. See package_launcher_lifecycle.md. Synthetic
observations confirm macOS config links can mutate package resources and Linux
partial resource copies can be accepted on rerun; both remain explicit next
repairs. Full runtime ownership/quotas/recovery, retirement, installed-platform
acceptance and canonical adoption remain open. No archive retry or historical
process reaping occurred.
