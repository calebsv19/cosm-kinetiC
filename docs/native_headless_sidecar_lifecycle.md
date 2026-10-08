# Native summary/progress admission

Main Edit now preflights both summary and progress destinations before allocating
or retaining an output root. Plans reject duplicate paths, linked/special/hardlinked
leaves, protected/source storage, scene-input overlap, reserved internal names,
and existing external files. Sidecars inside the output root must be direct
children; native cache/manifests keep their own subtrees. External sidecars remain
supported in an existing admitted parent, as needed by the detached job runner.
Existing internal files are only a preflight candidate: output-root admission
must preserve a completed predecessor, and fresh exclusive claim still applies.

After root admission, sidecars are exclusively created and flushed. Parent and
file descriptors pin their selected identities. Updates recheck the paths and
write through the owned descriptor, then flush and recheck completion. File or
parent replacement holds publication and preserves foreign replacement bytes.
Both sidecars must remain admitted before the output owner is marked complete.
Summary/progress JSON declares `artifact_class: operational_job`, so ordinary
clean holds it even if someone assigns a disposable-build claim.

The subsequent atomic-publication slice replaces live truncation with exclusive
staging and atomic generation replacement; see `native_headless_sidecar_atomic.md`.
Failed writes preserve the last published file and retain the stage. Running
owners and interrupted stages still require reconciliation. Cooperative checks
cannot prevent every uncooperative race or in-place foreign edit. Descriptor
witnesses are not cryptographic authentication. Detached runner status/request/
log writes still need their own lifecycle controls. No automatic deletion or
restart is introduced.

Validation: 42 focused/compiled/actual-CLI/cleanup/retention checks passed. The
supported CLI, Water, scene-project cache-output, runner smoke, runner policy and
runner bundle fixtures all passed on final source. The policy fixture exercises
normal overwrite and cancellation. Actual CLI checks verified that a bad plan
creates neither root nor duplicate sidecar and does not move an existing completed
run. Source headless/runner builds passed without new warnings. No historical
output, canonical source, installed package or independent archive was changed.

Run the native lifecycle proof locally with:

```sh
make BUILD_DIR=build/<fresh-profile> test-native-headless-sidecars
```

The target builds the selected source headless binary and explicitly passes its
path to the compiled and real-CLI tests. Broader integration commands remain in
AGENTS.md and the agent demo pack. Evidence:
`data/experiments/lifecycle-validation/20261007-native-sidecar-admission`.
New evidence remains outside the prepared independent backup.
