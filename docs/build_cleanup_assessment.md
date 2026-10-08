# PhysicsSim build and cleanup assessment

Assessed October 6, 2026 Pacific in the persistent Main Edit at base `dd55d7c`.
The canonical checkout at `3fa1ad5` remains unchanged. This is an assessment and
bounded disposable proof, not authorization to clean the real surviving tree,
change shared modules, package, deploy, publish, commit or adopt this lane.

## Overall state

The migrated local CFD lane is usable and substantially better protected:
configurable build/test/retained/tool roots, 19 declared reference support ABIs,
487 passing current reference checks, four passing evidence lifecycle checks,
fresh native/scene/manufactured evidence and portable sealed readback. Public
Water and scene-project cache-output smokes also pass in a fresh test root.
These checks establish their stated local scopes, not full physical force,
transient model, UI, installed product or release acceptance.

Cleanup is still a transitional arrangement. The new guard is a useful emergency
barrier, but does not yet enforce one complete owned-output lifecycle. It exists
only in uncommitted Main Edit; canonical `make clean` still invokes unguarded
recursive deletion. The surviving original build root should remain intact.

## Confirmed findings and recommended order

### 1. Validate every deletion path, not only BUILD_DIR — highest priority

`make/rules-runtime.mk:27` validates the build directory, then line 28 separately
deletes TARGET and six tool paths without sending them through the validator.
In a disposable checkout-shaped fixture, `TARGET=data/experiments` bypassed the
protected-root test and deleted a sentinel below protected retained storage.
The command completed with exit code zero. No real evidence was touched.

Replace the shell list with one cleanup planner that receives every exact path,
requires a declared disposable owner, validates protected-root overlap and path
components, refuses active owners, and revalidates identities before applying.
Provide a `clean-plan` readout. Normal clean should remove only disposable build
outputs; tests, operational runs, evidence, packages and archives need distinct
explicit lifecycle actions. Quote all selected paths. Include the session worker
and all actual root tools in the declared executable inventory: the current clean
recipe leaves `physics_sim_session_worker` behind, confirmed in a fixture.

Acceptance: protected-path overrides for every cleanup argument fail before any
delete; active-owner and symlink/overlap cases fail; the displayed plan equals
the applied set; an interrupted clean has an honest readback.

### 2. Replace filename-based evidence detection with owned artifact classes

`check_clean_root.py:22-31` recognizes particular filenames/directories, and
inspects hashes only when a JSON filename is exactly `receipt.json`. A disposable
`build/selected/accepted-result.json` containing `artifact_sha256` was deleted.
This confirms that the heuristic cannot guarantee arbitrary evidence preservation.

Keep the heuristic as a legacy hold, but declare separate roots and artifact
classes for objects/executables, fixture scratch, environment tools, retained
numerical runs, operational jobs, package staging and authenticated artifacts.
Make runners declare output ownership before execution. Retained data should
never be admitted under a disposable root; legacy unknown contents should cause
a review hold instead of being guessed disposable.

A text scan found build literals in 201 of 204 `run_cfd*.py` wrappers. This is a
classification queue, not proof all are active or all literals write output.
Migrate supported active workflows first; preserve historical wrappers and exact
checkpoint contracts as historical capsules rather than rebuilding every old
campaign. The graded reference wrappers still select build-local run and venv
paths, unlike the migrated active box/cube/manufactured lane.

Acceptance: each supported runner writes a fresh sealed run outside build/tmp;
clean and relocation preserve the entire inventory; no test quietly selects a
historical glob to establish current correctness.

### 3. Complete per-build and per-test isolation

Build roots differ, but app/tool outputs still share checkout-root names
(`rules-build.mk:154`, `sources-tools.mk:8-17`). Switching profiles or simultaneous
builds can therefore share published executables. A newer root binary can also
mask selection of an older completed build directory through timestamp logic.
That latter case is a source-grounded risk, not a separately reproduced full app
failure in this audit.

Seventeen of twenty integration shell fixtures contain recursive resets. Some
are safe unique temporary fixtures, but several have fixed roots and fixed job
IDs. The job-runner bundle smoke uses `build/agent_runs/jobs/ps-bundle-smoke-001`
and a shared `/private/tmp/physics_sim_job_runner_bundle_smoke`, deleting them
before a run. The migrated Water and scene fixtures honor TEST_TMP_DIR; other
fixtures require their own classification.

Place every real executable under its selected build/profile root. Make root
convenience entrypoints explicitly choose a verified artifact. Use unique
fixture identities and the configured test root for each invocation. Separate
operational job records from scratch fixtures and retained results. Add owner
locking only where shared publication or cleanup actually requires it.

Acceptance: two roots and two concurrent fixture invocations cannot overwrite
each other; selecting a root proves the executed binary belongs to it; no smoke
can erase a production/retained job or another invocation's diagnostics.

### 4. Track build configuration and publish outputs atomically

The ordinary object rule (`rules-build.mk:161-163`) depends on C source/header
dependencies, but not the complete compiler/flags/configuration identity. Using
that exact rule in a tiny disposable fixture, changing CFLAGS from VALUE=1 to
VALUE=2 reported no work and left object bytes unchanged. Similar risks apply to
link flags and support-library recipes; source/header timestamps alone cannot
identify all build inputs.

Add a stable fingerprint dependency for compiler identity, architecture, SDK,
compile/link flags, relevant Make configuration and imported dependency identity.
Keep profiles separate. Rebuild only when the relevant fingerprint changes.
Write outputs to staging names and atomically publish on success; use failure
cleanup appropriate to Make so an interrupted command cannot leave an accepted
partial executable.

Acceptance: flag/compiler/profile changes rebuild affected outputs; unchanged
identity is an incremental no-op; an interrupted build cannot appear successful
or replace the last verified artifact.

### 5. Put package and release cleanup under the same ownership discipline

`release-clean` directly removes RELEASE_DIR (`release.mk:19-21`), whose default
is `build/release` (`package-paths.mk:43-45`). Linux worker/desktop and macOS
package recipes also reset staging directories independently of the new clean
guard. Their paths can contain manifests, proofs and authenticated material;
this audit inspected the recipes without executing any package or release action.

Use job-scoped scratch staging for regeneration and explicit archive/readback
requirements for retained/authenticated artifacts. Check every selected output
root before reset; publish completed packages atomically. Route publication and
promotion through the existing release-control and Registry contracts rather
than introducing another authority system. Audit current refresh authority's
one-worktree condition against the persistent Main Edit model: it currently
refuses canonical refresh while any additional worktree is registered. Resolve
that policy with the owning lifecycle contract, without weakening ownership.

Acceptance: ordinary clean cannot delete retained releases; package staging can
be rebuilt without destroying predecessor evidence; installed/public/worker
versions and source versions remain distinct.

### 6. Make retention, recovery and operational status visible

Ignored `data/experiments` is intentionally outside Git; preserving source does
not back it up. Bundle manifests establish integrity, and their backup_verified
false value should remain unchanged: independent backup evidence belongs in a
separate receipt bound to an exact inventory and archive location. Later packets
are not automatically covered by an earlier archive batch.

Integrate the existing archive workflow with explicit copy-only intake,
checksum/readback, restore instructions and retrieval rehearsal before any
future pruning. Keep separate clean-build, current-test, qualification,
archive-audit and recovery checks. Provide a read-only local status/doctor
command showing resolved roots, binary/build identity, ownership, cleanup holds,
reference environment, current proof state and exact backup coverage.

Live sample diagnostics already have a finite 32-request retention policy in
`agent_session/service.py:666-672`. That policy should be visible to consumers:
observations needed as durable evidence must be copied/sealed explicitly before
routine live retention expires them. This is an operational contract review;
no sampling cleanup was performed.

Acceptance: an operator can see what clean will remove, why a root is held, which
checks are current, what evidence is backed up, and how to recover it without
reading historical chat logs. A recovery proof is distinct from checksum-only
archive preservation.

## Evidence and boundaries

Disposable cleanup/build reproductions and inspected sources are sealed in
`data/experiments/lifecycle-validation/20261007-cleanup-audit`. The assessment
introduced no runtime changes. It did not run real checkout cleanup, reset an
operational job, modify package/release state, or change canonical source.

Recommended first implementation slice: complete deletion-path validation and
owned executable inventory, with clean-plan and disposable acceptance. Next:
build fingerprints and executable isolation, then active runner/fixture roots,
then package retention and operator status. Canonical adoption should follow
validated owner review; until then its old cleanup recipe remains unchanged.

The backup completion and exact coverage are recorded separately in the lifecycle
repair status and the archive-copy receipt. The prepared batch covers original
survivors and fresh evidence captured before the subsequent public-headless,
archive-preparation and cleanup-assessment packets. It does not restore missing
pre-incident files or certify independent cube-reference physical accuracy.


## Backup result

The approved prepared snapshot now has a verified independent cold copy under
`/mnt/cold_archive_500g/codework-archive/generated-runs/physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a`.
All six declared payload checksums match and the fixed controller's read-only
reconciliation confirms ten exact intake/destination file inventories, with no
missing, extra or changed files. Both archive payloads were locally member-readback
verified. No local original or PC intake was deleted. Remote unpack/retrieval
rehearsal remains a distinct future proof. The completion receipt is separately
sealed in `data/experiments/lifecycle-validation/20261007-archive-completed`.

## Implementation progress after the audit

The findings above describe the audited pre-hardening behavior. Several are now
repaired in uncommitted Main Edit: all normal deletion arguments are planned and
validated, nested JSON receipts are held, the session worker is included, build
configuration drift triggers rebuilding, and control-only Make commands avoid
compiler/package setup. See [top-level hardening progress](top_level_hardening.md)
for passing checks, remaining gaps and exact adoption boundaries. Active build
ownership, executable isolation, atomic publication and explicit package/reset
paths remain open. Canonical cleanup has not adopted these repairs.
