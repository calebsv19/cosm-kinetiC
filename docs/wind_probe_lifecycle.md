# Wind orientation probe retained lifecycle

The legacy Wind probe removed an existing requested output_root recursively before
running, and keep_existing still reused scene/report names and passed overwrite
to the native worker. The repaired Main Edit tool treats output_root as an attempt
parent. Every run allocates orientation-<UUID>, preserving all existing content;
keep_existing is a deprecated compatibility flag with the same preserving behavior.
No reset or retirement is performed. Outputs now live under the printed attempt's
work/ directory, rather than directly under the requested parent.

The selected parent must be in this checkout's tmp, data/experiments or
visual_artifacts namespace. Source/build/tool storage and external parents refuse;
lifecycle lock/session namespaces and any ancestor with a bundle_manifest.json
also refuse. Symlink components are rejected. These are trusted-local cooperative
path rules, not hostile filesystem confinement or an upload sandbox.

Non-listing execution requires an executable headless worker under checkout/build.
The existing worker_execution hierarchy owns it through capture, execution and
acceptance, excluding cooperative rebuild and cleanup. The default is now
build/bin/physics_sim_headless; select another built profile explicitly. The old
root-level compatibility executable and external workers need their owning
lifecycle route and are refused here. Listing needs no worker or output admission
and creates no generated roots/locks. Scene JSON is bounded at 2 MiB and uses
existing strict numeric, duplicate-field and structure admission. Selected target
identity, finite orientation input, normalized case-name uniqueness and count /
resource arguments are admitted before attempts. Existing mesh-path resolution is
retained; referenced external assets and all runtime dependencies are not captured.

Each attempt retains immutable request, original scene, worker bytes/mode/hash,
selected control bytes/hashes, derived runtime scenes and per-case stdout/stderr /
outcome records. The existing retained-command supervisor executes workers with
inherited build/attempt ownership descriptors. The native output directory starts
absent; no overwrite flag or early log file makes it nonempty. Logs live separately.
Failed and uncertain attempts are retained, with failed/held receipts respectively.
The terminal receipt is created separately from the immutable running request.
Success is printed only after source/worker/control revalidation and terminal
receipt publication. Fresh report names cannot replace prior attempts. A partial
report or interrupted receipt is held evidence, not a completed recovery protocol.

The wall budget is at most 3,600 seconds (default 900), checked between cases and
passed as remaining time to each supervised command. Per-case sampled stdout /
stderr cap is at most 64 MiB. There are at most 32 orientations, 10,000 frames and
100,000 simulation steps per frame. Native result timeseries admission is 8 MiB /
10,000 records, with strict finite JSON and requested final frame checks; final
frame files are admitted as bounded regular nonempty files (64 MiB each).
These are local sampled bounds, not hard disk/RSS quotas, filesystem I/O deadlines
or complete workflow containment. Input capture, parsing, report work and teardown
can exceed the worker observation deadline. Full aggregate output/storage control
and escaped-descendant termination remain unqualified. No physical accuracy gate
or aerodynamic coefficient interpretation changes.

## Verification

Twenty-nine distinct methods pass: 21 actual-CLI lifecycle methods and eight
existing retained-supervisor regressions. The real control-only Make target repeats
the 21 methods with unavailable compiler/pkg-config and allocates no build profile.
Synthetic workers prove native output starts absent without overwrite, inherit
cleanup exclusion, preserve previous attempts and original inputs, retain failure /
timeout/log-bound diagnostics, reject protected/linked/sealed roots, hold competing
build ownership, keep listing read-only, preserve relative rotations, and refuse
ambiguous identities, malformed/nonfinite/oversized input, duplicate case names,
nonfinite/overlong timeseries and source drift. Source-drift failure does not print
success. Initial synthetic tests exposed a fixture string-quoting bug; its failed
log and persistent reproduction are retained alongside the repaired passing gates.

A fresh real headless build passes. The two-frame reduced-grid native attempt
finished headless execution but failed the unchanged nonzero-outlet metric check;
its input/logs and failed receipt remain. A separate 12-frame documented-grid native
baseline passes. The full existing portable Make fixture then passes baseline,
roll45 and roll90. Water and scene-project cache first-start fixtures also pass.
An initial concurrent first-start attempt was correctly refused while the native
probe held worker ownership; it succeeded after that verified operation ended.
The refusal log is retained, with no reset or duplicate running native attempt.

A bounded old-code control deletes only synthetic retained evidence before its
worker fails, demonstrating the previous loss exposure. Accepted baseline derived
scene bytes and legacy metric extraction match the repaired native output; this
is tool serialization/extraction parity, not an old-versus-new solver benchmark.
Matching build repeat preserves 237 objects/binaries and clean preview admits 474
owned files with no cleanup applied. The build retains a pre-existing signedness
warning in shape_asset_input.c; no new native warning is attributed to this Python
repair. Current builds/tests/proof sessions are terminal. Historical sanitizer
holds are separate and no reaping is claimed for them.

## Remaining work

This closes the probe's destructive reset and unsupervised command launch paths.
Complete process ownership, hard aggregate quotas, multi-file power-loss recovery,
retirement, independent newer backup and canonical adoption remain incomplete.
Packaging bundler temp ownership, read-only launcher inspection, installer/manifest
replacement policy and other inventory findings remain open. No private DragonWind
scene, GUI, installed app, public release or CFD physical-accuracy acceptance was
exercised. No commit, canonical adoption, installation, archive transfer or user
output deletion occurred. Full TL01-TL13 remains active.

Evidence: data/experiments/lifecycle-validation/20261007-wind-probe-lifecycle.
