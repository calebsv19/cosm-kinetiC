# PhysicsSim top-level hardening progress

Assessed October 6, 2026 Pacific. Scope: local source setup, build identity,
cleanup and artifact lifetime. Changes are uncommitted in persistent Main Edit
at base dd55d7c. Canonical at 3fa1ad5 has not adopted these changes.

## Backup and retained evidence

The approved prepared backup was copied through the existing cold-archive
controller. Six payload checksums match; ten intake/destination files match in
size and SHA-256. The historical archive has 6,391 verified members. Original
local files and remote intake remain intact.

Destination:
`/mnt/cold_archive_500g/codework-archive/generated-runs/physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a`.

The sealed completion receipt is in
`data/experiments/lifecycle-validation/20261007-archive-completed`.
Coverage is the prepared survivor and fresh-evidence snapshot. Later proof and
hardening packets are outside that payload. A later independent retrieval/restore rehearsal verified 6,391 historical files
and eleven fresh bundles; see `top_level_stability_assessment_20261007.md`. Missing pre-incident evidence is not restored by this copy.

## Implemented and verified locally

- Normal clean now uses one planner for the selected build root and every declared
  executable. Protected overrides, symlink paths, changed inventories, retained
  namespaces and nested JSON evidence markers cause a hold before deletion.
  The previously omitted session worker is in the inventory. `make clean-plan`
  gives a read-only inventory. Cleanup runs are serialized with each other.
- Compiler/tool identity, architecture, effective flags, output selection and
  Make fragments feed a configuration stamp. Configuration changes rebuild;
  unchanged configuration is an incremental no-op. Switching back to a previous
  configuration produces a fresh selection generation. Source-path output
  overrides are refused. Make removes failed targets via DELETE_ON_ERROR. Publication/control helper contents also participate in the configuration identity.
- Control-only goals load a small Make route: clean, clean-plan, status and the
  lifecycle regression tests work without compiler/library/package fragments.
- `make status` reports resolved roots, cleanup holds, configuration metadata,
  source versions and exact sealed backup receipt coverage. Binary profile,
  installed product and reference qualification remain explicitly unverified.
- CFD reference support libraries have declared rebuild rules and active retained
  evidence is outside disposable build/test roots. The two public headless
  fixtures honor the configured test root.

Validation: eight cleanup tests, one multi-case build-identity test, four evidence
lifecycle tests and two status tests pass. Fresh headless Water and scene-project
cache-output checks pass. The current reference cohort has 487 passing tests;
that run predates the final control-only Make routing edit. These are local
source and behavior checks, not installed, UI or physical model acceptance.

## Remaining slices and acceptance

| Priority | Improvement | Acceptance needed |
| --- | --- | --- |
| Complete for declared outputs | Isolate executable outputs by build/profile | Two-root builds and exported fixture binary selection passed; historical literal paths remain in the classification queue |
| 1 | Finish ownership coverage beyond top-level Make | Direct numerical builders and legacy output paths need declared ownership; normal Make now excludes clean, same-root and overlapping parent/child conflicts |
| 1 | Finish publication coverage and build ownership | Main object/link/tool/reference recipes now stage outputs; semantic dumps and historical experiment recipes remain open; root app copy was removed and Make ownership is active |
| Complete for ordinary build writers | Require exact disposable-output ownership | Compiler/dependency/configuration outputs have receipts; unknown or changed files hold cleanup. Direct and historical writers remain unregistered and held |
| 2 | Complete explicit fixture retention and pruning policy | All 20 integration scripts now allocate fresh capsules; bounded first-start/transport checks pass, while long/private visual proofs remain separately qualified |
| 2 | Guard package/release reset and staging recipes | Regeneration preserves predecessor evidence; every deletion path is checked under existing release authority |
| 3 | Classify remaining numerical wrappers | Supported workflows use retained roots; historical capsules keep exact inputs and assertion contracts |
| 3 | Finish provenance in status/doctor | Read-only prerequisite preflight is implemented; full compiler-input/profile identity remains explicitly unverified |
| 3 | Define recovery and retention contract | Restore rehearsal passes; finite live diagnostics are exported before expiry; exact backup coverage stays visible |
| 4 | Publish common top-level spec and validated adoption plan | Standalone setup, root rules, build identity, ownership and lifecycle commands have one documented contract |

The initial audit found build literals in 201 of 204 CFD wrappers and recursive
resets in 17 of 20 integration fixtures. These are classification queues, not
claims that every match is unsafe or supported. Package/release source recipes
were inspected; none were executed. The existing Main Edit/canonical refresh
policy needs owner reconciliation before adoption, without weakening authority.

## Operational guidance during transition

Use a separate selected build root under `build/` for ongoing work. Run status
and clean-plan before cleanup. A hold on the original build root is expected:
it contains historical retained evidence. Do not override that hold or run the
canonical checkout's old clean as a shortcut. No actual survivor cleanup,
canonical synchronization, commit, package, installation or release was performed.

The full source-grounded findings and disposable failure reproductions remain in
[build cleanup assessment](build_cleanup_assessment.md). This document tracks
implementation progress; it does not reinterpret historical proof as current.

## Atomic publication slice

`scripts/atomic_output.py` now stages the ordinary clang/fisiCs object rules,
app compiler links, declared tool/session links and all 19 reference support
libraries beside their destination, then replaces the final output only after
successful completion. Compiler failure and catchable interruption retain the
previous final artifact; staging is removed on those handled paths. SIGKILL can
leave abandoned staging directories, which are not accepted final outputs.

Dependency files use the original Make target spelling, not the temporary path.
They are published before the object; this is not an atomic transaction over
both files. Darwin dylib install identities use the final path. Five behavioral
checks cover real compile/link/dependency output, partial compiler failure,
SIGTERM, unsafe output refusal, and failed rebuild under the actual Make rule
with DELETE_ON_ERROR. The configuration test additionally verifies that editing
the publication helper invalidates the build selection.

This slice does not coordinate concurrent writers or cleanup. The convenience
app copy still uses cp; semantic dump and historical experiment recipes retain
their separate contracts. Executable isolation and owner coordination remain
required before claiming the full build lifecycle complete.

Incremental readback after the final headless build found no newer object files,
but `make -q physics_sim_headless` returns 1: the tool targets are explicitly
PHONY in rules-test-groups.mk and therefore relink on every invocation. This is
an existing graph issue to resolve with the next executable-isolation slice.
The incremental no-op proof currently applies to objects/configuration, not
all tool link commands.

## Executable isolation slice

Default app, CLI, session-worker and shape-tool outputs now belong to the selected
`BUILD_DIR/bin`; fisiCs has its separate `BUILD_DIR/fisics/bin` app output.
The app convenience copy is removed. Named Make commands are aliases for real
file targets, so they no longer force a tool relink. Legacy checkout-root binaries
are untouched and normal clean no longer selects them implicitly.

Integration shell consumers use explicit exported paths, including
PHYSICS_SIM_HEADLESS_BIN and PHYSICS_SIM_JOB_RUNNER_BIN. Direct fixture invocation
defaults to build/bin and no longer searches other builds or legacy root binaries.
The macOS package worker source copy uses SESSION_WORKER_BIN; package execution,
authentication and release state remain unverified. First-agent CLI docs now show
the build-owned default paths. Status reports selected build/bin executables;
this is path selection, not a full binary provenance certificate.

The concurrent two-profile behavioral fixture builds through the actual object
and headless link rules, returns different declared program results, preserves
both outputs when selecting either root again, and leaves a legacy root sentinel
unchanged. Build-versus-clean locking and same-root concurrent configuration
ownership remain open. Fixed fixture scratch/job roots also remain a separate
required slice. Historical executable-name overrides are not yet retired.

## Full Make ownership slice

Mutating top-level Make commands now enter `scripts/build_owner.py` before the
compiler/configuration graph is loaded. A shared checkout cleanup lock is held
for the complete Make process tree, and an exclusive selected-root lock refuses
a conflicting build of the same root. Different roots can own their builds
concurrently. Cleanup takes the checkout lock exclusively and reports a hold
without deleting outputs while any participating build is active.

Inherited descriptor identities are checked before recursive Make reuses
ownership. GNU Make jobserver pipes are forwarded for recursive -j operation.
Compiler children retain ownership descriptors, including if the supervisor and
publication helper are killed. Catchable interruption forwards to the child
process group; kernel locks release when their last participating process exits.
Persistent lock files alone are not live-owner evidence: status probes kernel
lock state without creating lock files.

Normal command aliases still avoid relinking current outputs. Read-only -n/-q
queries load the graph without creating an ownership operation. Mixed cleanup
and build goals are refused before mutation: run clean and build as separate
operations. Executable overrides outside the selected root, including known
legacy root names and shape tools, are refused during configuration admission.

Four process-level ownership tests cover live cleanup exclusion, same-root
conflict, independent roots, cleanup-to-build exclusion, signals, compiler lock
inheritance after supervisor/helper SIGKILL, exact overrides/multiple goals,
recursive -j forwarding and read-only Make behavior. Status has four passing
checks, including distinguishing a live lock from an unlocked persistent file.

Scope remains cooperative top-level Make operation. Direct external compilers,
legacy numerical scripts with independent compile paths, distinct overlapping
parent/child build roots, and raw historical recipes that write outside the
selected root still require classification or migration. No claim of a public
sandbox or complete adversarial filesystem transaction is made. Ordinary clean
exclusion is conservative across the whole checkout. Package/release deletion
logic and operational job ownership remain separate required work.

## Fixture isolation and resource slice

All 20 integration scripts now use the fresh-directory allocator, with a receipt
outside the work directory. Scratch parents are restricted to checkout/tmp;
explicit visual output uses visual_artifacts or data/experiments. An output
parent is never reset. Explicit legacy output overrides now select a parent for
a new child, not a directory to overwrite. Logs survive deliberate CLI overwrite
checks. Fixture job roots are inside their capsule, and bundle IDs are fresh;
operational build/agent_runs is untouched.

Three allocator tests cover concurrent unique allocation, parent preservation,
protected/outside/symlink refusal and explicit visual classification. Eleven
actual capsules cover duplicate concurrent Water and bundle runs, scene project,
CLI, all three job-runner fixtures and data/trace exports. The existing parent
sentinel remains intact. Shell syntax passes for all scripts; fixture assertion
command counts remain unchanged after excluding the two obsolete binary-location
fallback branches removed in the preceding isolation slice. Long/private Wind,
DragonWind and visual rendering behavior was not executed in this slice.

The initial transport checks inherited a desktop grid and wrote eight oversized
payloads totaling 1,245,610,159 bytes. Those exact task-generated files were copied
to a compressed local archive (1,396,573 bytes), SHA/size member-readback verified,
then removed as duplicate payloads. Receipts, logs, manifests and the parent
sentinel remain; affected capsules have explicit archive locators. This is local
compaction, not an independent backup or a restored field qualification.

CLI and job transport smokes now declare an 8³ grid. Five repeated CLI/job
capsules total 496,700 bytes and preserve the same output, rejection, overwrite,
transport, cancellation and job-report assertions. Cancellation still uses a
long declared workload with frame exports disabled. Ordinary clean does not
prune fixtures; retention/export/pruning requires its own explicit lifecycle.

## Package reset admission slice

Local Linux worker/desktop package assembly, local unsigned release artifact
creation, and initial macOS package assembly now validate every declared output
before staging. Existing directories, archives, checksum files, manifests and
failed-attempt reservations are retained regardless of their contents. A repeat
against the same used outputs is held; a fresh job-scoped attempt must come from
the owning release-control contract. No automatic predecessor deletion, archive
promotion or release authority is introduced.

Release-clean and both Linux package-clean recipes no longer delete existing
outputs. Fresh directory and file reservations live in .package-reservations
outside shipped payloads, protecting custom build namespaces and exact package
subdirectories from ordinary clean. The local_package_staging class is recognized
by cleanup independently of receipt filename. Failed or interrupted assembly
keeps its reservation and partial output for explicit recovery.

The Linux desktop determinism check now writes its second archive in a fresh
reserved comparison directory; it leaves the candidate archive and checksum
intact. This recipe change is source-inspected only; Linux execution remains
unqualified. Ownership UUIDs stay outside archive/app contents to avoid changing
artifact bytes and reproducibility.

Eight disposable behavioral checks cover complete-set admission, existing
signed/failed/unknown payloads, fresh reservations, source/escape/symlink refusal,
changed plans, unique comparison directories and the actual release/Linux clean
recipes. These checks do not authenticate any real package. No package build,
installation, signing, notarization, version update, Registry or publication ran.
The source versions remain program 0.4.0 and worker 0.3.4; public/registered
versions are separate and were not changed.

Remaining package work: complete transaction assembly/publication, typed reuse
of verified completed attempts, attempt-scoped proof roots, installed/Desktop
predecessor preservation, Main Edit refresh ownership reconciliation, and the
remaining audit/signing/notarization/export paths that write mutable proof or
archive names. Those are required before adopting this lifecycle operationally.
The fresh-output guard is a safety barrier, not a complete release mechanism.


## Overlapping build root ownership

Build ownership now acquires shared intention locks for every ancestor under
checkout/build and an exclusive lock for the selected root. Parent/child roots
conflict in both launch orders; sibling and unrelated roots remain independent.
Every hierarchy descriptor is validated for recursive Make reuse and forwarded
to compiler children, so ancestor exclusion survives supervisor/helper failure.
The read-only status command reports the complete admission hierarchy using the
required lock mode, distinguishing a conflicting parent from a sibling's shared
intention. Persistent lock files alone still do not establish active ownership.

Five process-level ownership tests and five atomic-publication tests pass,
including both overlap orders, checkout/build itself, sibling concurrency,
post-exit reuse and compiler-held ancestor exclusion after supervisor/helper
SIGKILL. Status tests additionally exercise exclusive versus shared ancestor
ownership. This closes the previously listed distinct overlapping root gap for
participating Make builds. Direct numerical compilers and historical recipes
outside the selected namespace remain separate migration work. Canonical has
not adopted this change; no real cleanup, package or release ran.

The full headless rebuild and both public Water/scene-project cache-output smokes
also pass after the hierarchy change. Evidence is retained in the sealed
`20261007-build-hierarchy` lifecycle-validation packet.


## Status resilience and non-regular cleanup inputs

The read-only local status command now bounds JSON metadata reads, refuses
symlinks/non-regular inputs without blocking, validates build digest/generation
and backup coverage fields, and reports malformed metadata per area while
preserving the rest of the report. Missing Git or a timed-out Git observation
produces source-status diagnostics. Invalid source-version text and executable
symlink components are unverified rather than silently trusted. Neither local
binary provenance nor installed/published state is inferred from these checks.

A FIFO fixture exposed a blocking JSON read in the cleanup inventory. Cleanup
now holds all special files and reads JSON through nonblocking, no-follow file
descriptors with regular-file and size checks. Package reservation reads receive
the same protection. No real cleanup was performed. The status command remains
read-only and earlier independent backup coverage remains exact.

Eight status, nine cleanup and eight package-admission checks pass. They cover
malformed/truncated JSON, invalid UTF-8, non-object/non-finite/deeply nested JSON,
size limits, invalid required fields, FIFO/directory/symlink inputs, unavailable
or timed-out Git, preservation of input bytes, and special-file cleanup holds.
An actual Main Edit status readback succeeds. Evidence is sealed in
`20261007-status-resilience`. Full binary provenance/environment readiness and
recovery qualification remain open. Canonical adoption has not occurred.


## Desktop predecessor preservation

Canonical/Desktop, Main Edit/Desktop and release Desktop refresh recipes now use
a journaled predecessor-preserving helper. Candidate and predecessor inventories
are verified before the old installed name is moved; completed and failed
attempts retain their payloads. Catchable publication failure restores the old
name. Unknown/unfinished history holds further refreshes. Source-drift rejection
keeps the failed Main Edit package, and package-remove holds existing staging.
Release Desktop refresh now requires the existing canonical authority gate.

Eight fake-app checks, eight package-admission checks, nine cleanup checks and
the existing Main Edit source contract check pass. The actual refresh recipe was
exercised in a disposable fake HOME with authority/build prerequisites stubbed;
no real app, signer or package build ran. The two-rename interval is recoverable
through retained copies/journal, not an atomic swap. Typed recovery and power-loss
qualification remain open, along with package assembly/reuse, proof-root lifetime
and refresh ownership reconciliation. See [Desktop replacement contract](desktop_replacement.md).
Evidence is sealed in `20261007-desktop-preservation`. Canonical is unchanged.


## Desktop exact-attempt recovery and crash boundaries

A read-only-default recovery CLI now consumes an exact retained Desktop attempt.
Application revalidates the plan under destination ownership, verifies available
predecessor inventories, restores only an absent name through a fresh copy, or
reconciles an already matching predecessor/candidate. The original attempt journal
is unchanged; separate recovery receipts bind its checksum. Failed recovery
staging is retained and retries use fresh identities. Completed recovery remains
terminal during later successful refreshes. Publication and rollback now require
native exclusive rename, refusing even occupied empty directories.

Fourteen Desktop tests, eight package-admission tests, nine cleanup tests and the
Main Edit source contract check pass. Actual child SIGKILL at all three rename
boundaries exercises recovery without running exception rollback. CLI plan/apply,
copy failure/retry, altered backups, occupied destinations and active locks are
covered in disposable fake homes. Evidence is sealed in `20261007-desktop-recovery`.
Power-loss durability, Linux exclusive-rename execution, real package/installed
acceptance, refresh authority reconciliation and package transaction/reuse remain
open. No real Desktop or canonical state was changed.


## Retained Linux package assembly and verified completed reuse

Linux worker/desktop source recipes now assemble through retained package
transactions, mapping declared output paths into fresh staging and reserving
final names. Publication copies/readback and native exclusive renames retain
original stages and failed attempts. Whole-set completed readback enables exact
reuse without rebuilding. Inputs, package metadata, selected tools and relevant
archive environment settings bind reuse. Worker checksum sidecars use portable
archive basenames. Existing host gates remain in place.

Twelve transaction, eight admission, nine cleanup and five ownership tests pass.
The actual worker recipes are exercised with fake binaries and stubbed host/build
prerequisites in a disposable root; recursive -j, mapped paths, sidecars and
verified repeat reuse pass. No real package or native Linux qualification ran.
Completed status proves assembly storage integrity, not package self-test,
authentication or release acceptance. Partial publication resume/recovery,
attempt-scoped proof roots and native desktop determinism remain required work.
See [package transaction contract](package_transaction.md). Evidence is sealed in
`20261007-package-transactions`. Canonical and production state remain unchanged.


## Exact retained package recovery

A read-only-default recovery command now selects an exact package attempt,
validates contract/reservation ownership and unchanged inputs, compares retained
verified staging and all existing published outputs, then fills only missing
names under cleanup/package exclusion. It never reassembles or overwrites an
occupied output. Separate recovery receipts bind the unchanged original attempt
journal; successful recovery enables ordinary exact reuse. Unverified failed
assembly remains held rather than inferring completeness from partial files.

Twenty transaction, eight admission, nine cleanup and five ownership checks pass.
Actual child SIGKILL before/after original publication and during recovery proves
local process-interruption recovery from observed files rather than checkpoint
claims. Copy failures and altered/occupied inputs stay retained and held. Evidence
is sealed in `20261007-package-recovery`. Machine power-loss durability, native
Linux/desktop package qualification, attempt-scoped proof lifetimes and refresh
ownership reconciliation remain open. Canonical and production state are unchanged.


## Fresh retained package proof lifetimes

Package self-tests, worker validation/dry-run, desktop determinism and Main Edit
process-audit now run in fresh retained proof capsules. Work/log/receipt paths are
separate, explicit mappings replace fixed scratch outputs, and success is retained
as well as failure. Package input/control/executor identities bind each receipt;
input drift and active cleanup/package mutation hold proof. The package_proof
class is excluded from normal clean. macOS session validation preserves MCP
requests, stdout/stderr and timeout diagnostics under the selected capsule.

Nine proof, twenty transaction, eight admission, nine cleanup and five ownership
checks and the Main Edit source contract pass. Concurrent capsules and two actual
macOS self-test recipe invocations use fake payloads; no real package or refresh
ran. A bounded source scan finds no remaining recursive shell deletion in the
package/release Make fragments. This is not complete release immutability or
arbitrary-script cleanup coverage. See [package proof lifecycle](package_proof_lifecycle.md).
Evidence is sealed in `20261007-package-proof-lifetime`. Full artifact ownership,
release audit/signing proof lifetimes, refresh authority reconciliation, native
qualification and canonical adoption remain open.


## Structured Main Edit refresh observation

The Main Edit grep gate is replaced by exact, fresh typed MEW1 process-audit
validation. Complete selected-path fallback, matching app/path scope, empty
matches, boolean state and no truncation/error are required; malformed, ambiguous,
stale and running receipts hold replacement. Bounded metadata reads now refuse
duplicate/non-finite JSON, and status also refuses duplicate fields. Shared MEW1
and canonical refresh ownership policy are unchanged.

Five process, eight status, nine cleanup, nine proof, twenty transaction and
fourteen Desktop checks plus Main Edit contract pass (65 checks). An actual
bounded read-only MEW1 audit was accepted at observation time and retained. The
saved receipt is historical, not permission for a later operation. No real app
replacement or shared producer mutation ran. Evidence is sealed in
`20261007-process-audit-guard`. Process launch races, refresh ownership policy,
full artifact classification, release proof lifetimes, native qualification and
canonical adoption remain separate open requirements.


## Explicit disposable build ownership

Successful ordinary compiler publication now records exact per-file ownership
outside the build tree in `tmp/build-output-ownership/`. Dependency outputs and
configuration stamps/active selection metadata receive their own receipts.
Receipts bind the checkout-relative path, producer class, SHA-256 and file
identity (including inode, size, mode, modification and change timestamps).
Only build namespaces and explicit legacy compiler entrypoints may be recorded.
There is no automatic adoption of existing files or historical build contents.

Normal clean uses schema `physics_sim_clean_plan_v2` with a verified full file
inventory. Unknown regular files, unknown empty subdirectories, missing or
malformed receipts, replaced files and content mutations cause a hold before any
removal. Nested mutations now participate in whole-plan revalidation. A compiler
killed before successful publication can leave staging material; that material
is held rather than inferred disposable. Receipts survive clean; loss of this
scratch metadata causes a conservative cleanup hold, not permission to delete.

This covers the ordinary atomic compiler path and configuration writer. Direct
numerical builders, semantic dumps and historical recipes still need migration;
their unregistered contents deliberately prevent whole-root cleanup. This is a
trusted local cooperative contract, not a sandbox against arbitrary concurrent
filesystem mutation or a receipt-authentication service. The canonical checkout
has not adopted it. Historical original build contents remain untouched.

Validation of this slice: 34 affected build/cleanup/status/isolation tests and
56 package/Desktop lifecycle regression tests passed. A fresh real headless
build and both public first-start fixtures passed. The final real clean-plan
verified 454 files; an unknown sentinel prevented all removal, then removing
only the task-created sentinel allowed cleanup of that fresh proof build.
The historical CFD build tree remains held. Evidence is sealed in
`data/experiments/lifecycle-validation/20261007-disposable-output-ownership`.
This packet is outside the prepared independent backup's coverage.


## Retained semantic and native compiler publication

All 18 semantic-dump recipes now allocate fresh retained capsules instead of
writing fixed object/log paths. Success and failure attempts preserve diagnostics,
source capture, compiler/control identity and sealed readback. Direct helper
invocations acquire normal build ownership; compiler children inherit it.
Legacy `SEMA_*` paths are validated hints and remain untouched. Full transitive
compiler-input capture is still open and is explicitly false in receipts.

The six active retained CFD compiler paths now use bounded candidate compilation
and atomic no-replace publication. Failed and competing attempts retain their
candidates and logs; existing probe binaries are never overwritten. These are
retained proof artifacts, not disposable build outputs. Both artifact classes
are explicitly held by normal cleanup. Historical wrappers remain unchanged.

Validation: a 39-test compiler/build/cleanup/native lifecycle cohort passed;
10 final semantic/retained-compiler tests passed after compiler identity binding;
23 cleanup/status checks passed after artifact-class protection. These cohorts
include overlapping tests and are not a full repository suite. All 18 recipe
bodies were exercised with a controlled compiler. Two real fisiCs Fluid2D attempts
passed and their sealed capsules survived selected-root cleanup unchanged. A
fresh 16x8x8 native box run completed with full SI field readback and verified its
33-file sealed bundle. This is local lifecycle proof, not additional physical
qualification, installed-package acceptance, or canonical adoption.

See [retained compiler lifecycle](semantic_proof_lifecycle.md). Evidence is sealed
in `20261007-retained-compiler-lifecycle`; this later packet is outside the
prepared independent backup. Ordinary disposable ownership coverage, supported
fixture isolation and the active retained compiler paths have now advanced.
Historical writer classification, broader per-command provenance, retention and
recovery policy, setup/doctor, refresh policy and canonical adoption remain open.


## Retained evidence admission and readback

Retained-root selection now rejects overlaps with source/header/script/test/docs,
Make/configuration, vendored dependencies, Git/agent metadata, exports/dist,
visual output, tools and configured build/test/reference-tool roots. Both active
readback CLIs use this admission for their explicit output destinations. Root
symlink components are refused after known macOS system-alias normalization.

Sealing and verification refuse special files anywhere in a bundle, including
FIFOs named as manifests or service locks. Empty bundles cannot publish a seal.
Manifest reads are bounded, reject duplicate/non-finite JSON and validate schema,
boolean flags, canonical artifact paths, SHA-256 strings and integer sizes.
Digesting verifies regular-file identities; final readback rechecks inspected
identities and inventory so an earlier artifact cannot change unnoticed during
later hashing. Invalid readback CLI input returns a concise hold diagnostic.
The v1 regular `service.lock` exception and file-only inventory remain explicit;
this is integrity readback, not a hostile-write sandbox or filesystem snapshot.

Thirty affected evidence/semantic/compiler/status tests passed, including actual
native and readback CLI source-root refusals, oversized sparse-manifest refusal,
and a simulated mutation during multi-file readback. Readback CLI guard fixtures
use a controlled unused numerical import; they do not run numerical methods.
Four native retain/relocate/clean/rebuild lifecycle checks passed separately.
All 22 prior sealed lifecycle packets passed stricter readback unchanged. A fresh
16x8x8 native box run completed and its 34-file bundle passed readback. Status
still holds the historical build tree and verifies the prepared backup receipt.
No original evidence was modified and no canonical adoption occurred.

The active source-freeze declarations include the shared strict-JSON parser.
The owning lifecycle doc now reports completed prepared-backup coverage correctly.
Evidence is sealed in `20261007-retained-evidence-admission`; it is outside that
prepared independent backup. Broader compiler provenance, specialized writer
classification, setup/doctor, retention/recovery policy and adoption remain open.


## Read-only local setup doctor

`make doctor` and `DOCTOR_PROFILE=cfd-reference` now inspect configured-root
separation, the listed portable checkout baseline, compiler/Make/pkg-config,
existing macOS target dependency selection/SDK, json-c compatibility-archive
presence, local metadata, exact reference distribution and runtime versions,
import origins, disposable-output byte ownership, cleanup holds and backup
coverage. Control-only routing does not load compiler/link/package fragments.
No installation, build, fixture allocation, canonical adoption or network request
is performed. Exit 0 is readiness for a build attempt, not qualified build/test,
ABI, installed/public or physical acceptance. Those flags remain false.

Status previously selected legacy checkout-root executables even for an isolated
build root. It now uses selected profile outputs by default; Make forwards exact
explicit output selections to both status and doctor. Unknown explicitly selected
legacy files remain held. Missing/malformed metadata stays visible rather than
being hidden by tool availability. Reference import versions must match the
tracked distribution pins. Read-only probes have wall/output bounds and reap
process groups, including children that hold pipes after the parent exits.

Thirty-seven doctor/status/cleanup behavioral tests and twelve build ownership,
isolation, identity and publication routing checks passed. The actual control-only
Make fixture confirms no file changes and no generated roots, without loading
build fragments. Live fresh-root headless and reference profiles report
`ready_for_build_attempt`; the historical root correctly reports
`attention_required` and remains held. Runtime and distribution versions match:
scikit-fem 12.0.2, NumPy 2.5.3, SciPy 1.18.1, pyamg 5.3.0. No solver qualification
was rerun or inferred. The fresh doctor root was not created.

See [local doctor](local_doctor.md). Evidence is sealed in
`20261007-local-doctor`; it is outside the prepared independent backup. Full source
and compiler dependency identity, specialized writer classification, environment
setup lifetime, retention/recovery policy, common spec and canonical adoption
remain required work.


## Retained reference environment setup

Reference setup now plans without writes, reuses exact matching environments,
and holds mismatched or incomplete prefixes. Fresh creation retains requirements,
commands, logs and terminal state under a unique attempt identity. Base-to-AMG
in-place upgrades require a fresh tools root. Doctor refuses incomplete managed
state before package imports. Environment receipts are explicitly retained classes.

Forty-six setup/doctor/status/cleanup checks passed. Controlled fresh success and
real final-prefix venv creation with deliberate offline failure were exercised.
Both live targets reused the existing full environment; all 5,161 file and
symlink entries remained unchanged. No network installation or solver acceptance
was inferred. See [reference setup lifecycle](reference_environment_lifecycle.md).
Specialized writers, full dependency provenance, retention/recovery policy, the
common spec and canonical adoption remain open.

Evidence for this slice is sealed in `20261007-reference-environment-lifetime`;
it is outside the prepared independent backup.


## Compiler predecessor admission and ordinary contract writers

Staged publication now refuses unknown or changed destinations before compilation
and rechecks output/dependency identities before replacement. Ordinary builds
cannot silently adopt pre-existing bytes into disposable ownership. Select a
fresh root when predecessor admission is held. Cooperative Make ownership still
provides exclusion; this is not malicious-filesystem or multi-file power-loss
qualification.

Twenty-six compiler commands across 24 ordinary session/solver and 2D contract
targets migrated to the wrapper. Twenty-six lifecycle/regression checks passed,
including all actual migrated recipe bodies. Five native contracts passed in a
fresh selected root. Five binary copies were retained and hash-checked; clean-plan
and guarded clean then removed that fresh registered root. No original evidence
or historical build root was touched. See [compiler output lifecycle](compiler_output_lifecycle.md).
Evidence is sealed in `20261007-compiler-predecessor-admission`, outside the
prepared independent backup. Historical numerical writers, configuration writer
admission, retention/recovery, common spec and adoption remain open.


## Configuration predecessor and interrupted-state admission

Build identity now verifies ownership of active state and the selected stamp at
Make graph selection, including existing stamps whose recipes would be skipped.
Publication accepts only the exact selected metadata path and rechecks prior
identities. Active selection is bounded strict JSON with a valid digest and
integer generation. Unknown, changed, malformed and special metadata are held.
Missing active selection beside existing configuration state is held rather than
resetting the generation and risking stale-object reuse. Matching state is a
no-op. Select a fresh root; there is no implicit historical adoption or recovery.

Fifteen admission/publication/identity/isolation checks passed, including actual
Make graph holds and incremental generation behavior. The supported real headless
build, Water smoke and scene-project cache-output fixture passed against the
final helper in an isolated root. See [configuration lifecycle](build_configuration_lifecycle.md).
Evidence is sealed in `20261007-configuration-predecessor-admission`, outside the
prepared independent backup. Compiler identity probe bounds, full transitive
provenance, historical writers, retention/recovery, common spec and canonical
adoption remain open. No historical build was cleaned or canonical source adopted.


## Bounded compiler and SDK identity probes

Doctor and build identity now share one local bounded probe runner. Version and
SDK probes have a 15-second wall bound and combined 64-KiB captured-output bound,
with no shell. Failed and successful probes terminate owned descendants, including
children that closed captured pipes after a successful parent. Compiler and SDK
selector executable fingerprints must match across probing. The helper bytes and
SDK selector hash participate in configuration identity.

Thirty-four probe/doctor/configuration/identity/isolation/reference-setup checks
passed. The supported headless build and Water/scene-project first proofs passed
against the final helper. Live reference doctor remained read-only and did not
create its selected root. See [tool identity probes](tool_identity_probes.md).
Evidence is sealed in `20261007-bounded-tool-identity`, outside the prepared
independent backup. Full transitive provenance, historical writers, retention and
recovery, common spec and canonical adoption remain open. No original evidence,
canonical source, installed app or published state was changed.


## Retained box contract recipes and pilot lifecycle specification

Six box contract/material/session recipes now compile and run in fresh retained
capsules. Previous build binary/log paths remain untouched provenance hints.
Declared C inputs, public/adjacent headers and recursive local quoted includes
are frozen, including included C files; control helpers are frozen too. Compiler,
source, control and binary identities are rechecked. Commands have wall and sampled
log bounds, retain failures, reap their groups and inherit build-owner descriptors.
The retained proof class holds ordinary cleanup. Full external dependency capture
and hard disk quotas remain separate qualifications.

Twenty-seven lifecycle/regression tests passed, including all six actual recipe
bodies. All six real native box/material/session contracts passed, including
three sanitizer variants, and their sealed readback passed. The initial material
capture failure remains sealed and retained; corrected attempts have new IDs.
See [native contract proof lifecycle](native_contract_proof_lifecycle.md). The
validation packet `20261007-retained-box-contracts` binds the six owning retained
capsule manifests; the capsules remain separately retained, not duplicated into
that packet. All later artifacts remain outside the prepared independent backup.

The [pilot lifecycle specification](top_level_lifecycle_spec.md) defines thirteen
explicit requirements and current evidence/coverage gaps. It is a completion
checklist, not a claim of repository-wide acceptance. Remaining historical writers,
retention and recovery, transitive identity and canonical adoption are open.
No original evidence was cleaned and no canonical, installed or published state
was changed.


## Versioned retention policy and bounded read-only inventory

A machine-readable retention policy now distinguishes disposable build products,
scratch, retained evidence/proofs, environments, operational jobs and package or
recovery-controlled artifacts. Unknown contents are held. Age and passed metadata
never establish deletion eligibility. `make retention-audit` uses the control-only
route to inventory selected lifecycle roots with bounded enumeration, metadata
reads and wall time. Symlink leaves are counted without following, special files
are not opened, and all integrity/archive/terminal/pruning claims remain false.

Six behavioral tests passed, including actual Make routing with no generated roots
or file changes. Live inventory correctly held mixed historical build storage,
counted four environment symlinks and left oversized receipts or bounded-out roots
incomplete. Exit 2 is honest incomplete/held reporting, not an audit implementation
failure. Logical bytes are partial observations, not reclaimable physical storage.
See [artifact retention policy](artifact_retention_policy.md). Evidence is sealed
in `20261007-retention-policy-audit`, outside the prepared independent backup.
TL11 now has executable policy/inventory coverage; exact retirement identity,
terminal-state evidence, archive coverage matching, restore proof and guarded
pruning remain open. Other historical writers and canonical adoption remain open.
No source evidence, environment, build or package was pruned.


## Fixture lifetime supervision and bound terminal evidence

Twenty fixture entrypoints now enter a unique lifetime supervisor before their
work. It holds kernel ownership, passes a high-numbered descriptor through shell
allocation, preserves Make ownership/jobserver descriptors and supervises an owned
process group. Original allocation records bind to the invocation; separate
terminal records bind owner and session receipt checksums without overwriting
previous output. Stale JSON without the actual locked descriptor cannot prove
active ownership. Termination refusal is retained as a failed unverified reap,
not lost terminal evidence or a false success. Forced termination stays held.

Fifteen session/allocation/retention behavioral tests passed; the six session
checks passed three consecutive runs after the signal edge case was repaired.
Real Water, scene-cache and detached job-runner bundle fixtures passed under final
supervision. Heavy/private fixtures were not all rerun. See [fixture session lifecycle](fixture_session_lifecycle.md).
Retention policy version 2 includes invocation records. Evidence is sealed in
`20261007-fixture-session-lifetime`, outside the prepared independent backup.
Payload identity, escaped descendants, archive coverage and pruning eligibility
remain unverified; terminal metadata does not substitute for a retirement plan.
Historical writers, recovery proof and canonical adoption also remain open.
No original fixture or other artifact was pruned.


## Independent prepared-snapshot retrieval and restore

The existing cold archive volume UUID was checked before and after read-only
retrieval of the six prepared payload files. All payload hashes matched. Fresh
local extraction restored 6,391 historical files with exact survivor-manifest
path/size/SHA-256 agreement, plus 560 fresh files and eleven sealed bundle
readbacks. Originals, remote archive and intake remain unchanged. No restored
binary was executed and missing pre-incident files remain missing.

A create-only bounded restore helper rejects overwrites, unsafe members and
checksum drift. Five restore behavior checks, ten status checks and thirteen
doctor regressions passed. Status attaches restore evidence only to the exact
sealed copy receipt hash, archive destination and payload map; historical copy
receipts remain immutable and general current-evidence coverage remains false.
See [archive restore rehearsal](archive_restore_rehearsal.md). Evidence is sealed
in `20261007-independent-restore-rehearsal`; retrieved payload and expanded trees
remain separately retained under the documented temporary rehearsal identity.
Prepared-snapshot recovery is now proven. Later backup coverage, retirement and
pruning, remaining historical writers and canonical adoption remain open.

## Refined contract writer migration

Twelve stdout-only refined CFD recipe bodies now honor `BUILD_DIR` and publish
through the existing guarded compiler wrapper. Mixed coupling/result recipes
that produce JSON evidence remain held for retained-run migration. No numerical
flags or assertions changed. The controlled recipe fixture exercised 36 total
migrated targets and 38 registered outputs, with failure/unknown predecessor
checks for both ordinary channel and refined mesh. Eight compiler behavior and
fifteen cleanup regression checks passed. Nine real bounded native refined
contracts completed successfully in a fresh selected root; their individual
physical gates retain original results, including not-qualified cases.

The fresh sealed packet `20261007-refined-contract-writers` contains 222 files,
manifest SHA-256 `0d377f5d211fdc93f204dee67b864ca4dadf2d0b73e4a77aa0a1439c5b6d577d`.
It retains source snapshots, logs and hash-checked binaries and exact disposable
output inventory. Source snapshots are bounded inputs, not full transitive
environment reproduction. The packet and build remain local; neither is covered
by the prepared archive. No original build cleanup, canonical adoption, commit,
package or release occurred. Other specialized and historical writers remain.

## Pressure/material and atmosphere compiler writers

Twelve pressure-trace API, anisotropic pressure trace, material-scaling and
passive/evolving/open atmosphere normal/sanitizer recipe bodies now use selected
`BUILD_DIR` paths and guarded compiler publication. The controlled recipe proof
executes 48 migrated targets with 50 registered outputs and checks compiler
failure/predecessor holds in five families. Eight atomic-output tests and one
actual build-isolation test passed. All twelve real native targets completed
with exit zero in a fresh root, including six sanitizer variants. Numerical
source, flags and assertions were unchanged; this does not certify cube force
accuracy or whole-program acceptance.

The sealed packet `20261007-atmosphere-contract-writers` verifies 229 files,
manifest SHA-256 `8006814987a38c0cc4d6dd8db124f3dae431c075ca4c566d0aefb3af58857bac`.
It retains compiler commands, logs, source snapshots, twelve hash-checked
binaries and the exact disposable inventory. Full transitive environment
reproduction and later-evidence archive coverage remain unverified. Neither
original nor selected build root was cleaned. Canonical adoption remains open.

Three atmosphere worker builders and nine script/test files still select fixed
worker paths. Their builder and consumer routes require coordinated selected-root
migration; periodic JSONL and convergence reports need retained-run identities.
No package, installed, Registry, release, commit or canonical changes occurred.

## Coordinated atmosphere worker selection

Three atmosphere worker builders now publish atomically inside selected
`BUILD_DIR` paths and export exact absolute consumer paths. Three CLI defaults
and six direct/coupled test consumers share app-local selection; convergence
inherits selected open worker through its existing import. Invalid environment
paths hold, and missing selected binaries never fall back to another build.
Three real worker builds and six Make suites passed (39 consumer tests and
three native prerequisites). Three selection checks passed through control-only
Make. Three finite readbacks matched selected worker SHA-256 values.

The sealed `20261007-atmosphere-worker-selection` packet verifies 230 files,
manifest SHA-256 `124927a40c5e3923949366303fa7d5c78a7e6936a12591d40a08bc0257eef0f7`.
Later archive coverage, standalone caller ownership, bounded subprocess output
and transitive library provenance remain open. Fixed-path evidence reports need
retained-run migration. Build roots and original evidence remain retained; no
canonical adoption, commit, package, installation or release occurred.

## Bounded atmosphere worker capture

Three atmosphere adapters now share the bounded capture engine with identity
probes. Limits apply while reading pipes: native stdout defaults to 64 MiB
(open requests preserve their existing explicit maximum of 256 MiB), stderr
64 KiB and execution 120 seconds. Raw protocol bytes and existing numerical
acceptance remain. Capture forwards interruption, reaps its owned process group
and passes inherited Make ownership descriptors. Refused signalling or a final
child wait exceeding five seconds returns unverified and native acceptance holds.

An initial SIGTERM test exposed a macOS PermissionError during post-exit group
teardown. That diagnostic is retained; the final controlled-refusal and live
interruption checks pass with honest hold behavior. Six capture checks, five
probe checks, thirteen doctor checks and one actual build-identity check passed.
Final real Make consumer suites passed 39 tests and three native prerequisites.
The sealed `20261007-worker-capture-bounds` packet verifies 20 files, manifest
SHA-256 `a5b9a3eee2f33548d4356d189d9cf35178f34a9189d67c1ec0785c7a94af85cd`.

Standalone acquisition of execution ownership, escaped process groups, forced
termination and overall adapter memory remain outside this qualification. An
additional source audit found the newly selected atmosphere worker file targets
are not yet in the configuration-stamp prerequisite list; compiler/library
configuration rebuild coverage needs a dedicated repair and actual Make proof.
Fixed-path retained reports, transitive provenance and canonical adoption remain
open. No original cleanup, commit, package, install or release occurred.

## Atmosphere worker configuration rebuild coverage

The three atmosphere workers now depend on configuration selection stamps, with
forced rebuilding on a newly selected stamp to avoid whole-second timestamp
ambiguity. Worker destinations and JSON-C compile/link flags are tracked and
admitted by build identity. Recipes consume configured JSON flags directly.
The actual recipe fixture (`python3 -B tests/test_atmosphere_worker_configuration.py`)
passed flag/link/compiler drift, matching no-op, all three protected-output holds
and changed-predecessor refusal. Ordinary identity, five admission and one
isolation regressions also passed.

All three real workers built in fresh `build/lifecycle-worker-configuration-20261007a`;
a repeated build preserved exact bytes and nanosecond mtimes. Six real consumer
suites passed 39 tests and three native prerequisites. The sealed
`20261007-atmosphere-worker-configuration` packet verifies 231 files, manifest
SHA-256 `dc131d583168bbb5847671df7c71074adebeab68760397d4607e527d7071a2c1`.
It retains source snapshots, logs, no-op readback and verified build inventory.
External JSON-C header/library binary provenance remains incomplete; tracked
flags do not prove dependency bytes. Standalone ownership, retained reports,
retirement and canonical adoption remain open. No cleanup, commit, package,
installed or release action occurred. Later packet archive coverage remains open.

## Full-lifetime atmosphere execution ownership

All three Python run adapters acquire or borrow cooperative ownership before
worker hashing and retain it through execution and final acceptance. Direct runs
lock the selected worker subtree with global cleanup/ancestor exclusion; sibling
roots remain independent. Make owners are borrowed only after descriptor identity,
no-symlink, kernel-witness and claimed-FD lock reaffirmation checks. Worker capture
passes descriptors onward. External/symlink/missing workers hold; other checkouts
and external binaries need their owning lifecycle.

The first native retry exposed a nested passive CLI launcher closing descriptors
under its owning Make process. Forwarding descriptors fixed the hold. A stale
subprocess exception import from capture refactoring was also restored in all
three CLI error handlers. Initial failed diagnostics remain retained. Final five
execution ownership, five build ownership, six capture and eight atomic tests
passed. Final six native suites passed 39 tests plus three native prerequisites;
three separate standalone requests accepted exact worker SHA readbacks and
released their contexts. A concurrent standalone call against the active Make
root was correctly held; the final standalone proof used an independent root.

The sealed `20261007-atmosphere-execution-ownership` packet verifies 247 files,
manifest SHA-256 `51bce18d4be6ae51cffe979504fac86185668edf143d70172e0361ef5dc61176`.
Forced termination recovery, escaped descendants, full transitive provenance,
remaining standalone writer families, retained reports, retirement and canonical
adoption remain open. Later archive coverage remains unverified. No cleanup,
commit, package, installation or release occurred.

## Retained atmosphere convergence reports

The Make convergence goal no longer writes fixed build metrics. It runs a
bounded retained report supervisor under worker ownership and allocates a fresh
UUID capsule beneath `EXPERIMENT_DIR/report-proofs`. Request/terminal receipts,
metrics, bounded child logs, declared Python snapshots, interpreter/worker hashes
and a worker copy are sealed. Input drift, child failure or invalid report contract
retains a failed attempt. Direct legacy `--report` refuses existing outputs and
protected paths before tests and uses exclusive publication. Analytic assertions
and metric definitions remain unchanged.

Four actual report CLI fixtures and twelve evidence admission checks passed.
Relocation readback passed and changed metrics were refused. Two real Make runs
each passed three analytic tests and produced distinct sealed capsules; both
were reverified afterward. The lifecycle packet
`20261007-retained-convergence-report` verifies ten files, manifest SHA-256
`8dc5b5a5bb7eae95cccf366491bc38dcbda5bc75fbc2e4e7ff9ec4137edbbdeb`, and
binds the separately retained native capsules. They are outside the prepared
backup. No legacy report or original build was removed, and no canonical, commit,
package, installation or release action occurred. Fixed periodic JSONL, mixed
refinement and other historical report writers still need migration; complete
transitive reproduction, retirement and canonical adoption remain open.

## Periodic retained JSONL and verified terminal sealing

Normal and sanitizer unforced periodic Make recipes now use fresh contract
capsules with immutable `results.jsonl` / `sanitizer.jsonl`. The normal recipe
executes the unchanged assessment from its frozen snapshot; the sanitizer keeps
its original `small` argument. JSONL is exclusively copied from retained stdout,
hashed and rechecked after assessment. Python assessment ignores environment
optimization; numerical source, flags and assertion gates remain unchanged.

Contract teardown has a bounded final wait and uncertain signalling/wait state
leaves the receipt failed and unsealed. Bounded capture now exposes terminal
verification explicitly; convergence reports also leave uncertain-child attempts
unsealed. Ten contract, five report, six capture and five probe checks passed.
Final normal/sanitizer native recipes passed, including the original normal
physical-refinement assessment; convergence passed its three analytic tests.
The read-only clean plan for the fresh periodic build root excludes all retained
JSONL/capsules. No cleanup was applied.

The sealed `20261007-periodic-retained-jsonl` packet verifies eighteen files,
manifest SHA-256 `253da7ba92d194a12fb4d3cf9b3a7b1d1c3ee5f93711b9f933e689fba420f807`.
It binds three separately verified native capsules, assessment output and clean
separation evidence. Early warning-bearing attempts remain retained; the regex
warning was corrected before final qualification. No legacy output, canonical,
commit, package, installed or release mutation occurred. Mixed refinement and
historical writers, full transitive provenance, retirement, archive coverage
of later packets and canonical adoption remain open.

## Mixed-refinement retention and input integrity

Native/matrix resolution series now retain fresh 8/16/32 JSON in immutable
contract capsules. Coupling assessment builds its own fresh native companion;
legacy build-local JSON is neither selected nor replaced. Final integrity checks
cover source/control snapshots, selected interpreter/config, assessment data and
nested native bundles. Partial-resolution failure retains earlier output and
stops the series; uncertain companion teardown leaves the outer attempt unsealed.

Seventeen contract lifecycle checks and both final real Make recipes passed.
The sealed `20261007-refined-retained-series` packet verifies 9 files,
manifest SHA-256 `b64cebab008b50071fcb0e70e6be8f7ee5834c69bb4d352123258082747befb2`. Actual native comparison and time
refinement gates passed unchanged. Later independent backup coverage, historical
writers, full dependency provenance, retirement and canonical adoption remain
open. No original cleanup, commit, package, installation or release occurred.

## Package-proof command limits and terminal receipts

Package proof execution now reuses the bounded retained contract runner with
900-second default wall and 64-MiB sampled combined-log limits. Exclusive stdout
and stderr remain retained. Catchable signals reap owned children; uncertain
teardown leaves terminal verification false and avoids active-work inventory.
Known exit statuses persist; special/unreadable work still records a failed
terminal receipt. Errors identify the retained capsule.

Fourteen package-proof checks and seventeen contract regressions passed. The
actual CLI retained controlled success, failure, timeout and overflow attempts.
The sealed `20261007-package-proof-command-bounds` packet verifies 22
files, manifest SHA-256 `2a331c17aa9c1370501e21509cc6de5e22ddb92b146a988e0132e7bbfed812c0`. This qualifies command
supervision, not inventory resource bounds, hard disk quotas, forced supervisor
death, escaped descendants or real platform/package acceptance. No original
cleanup, canonical adoption, commit, package build, signing, installation or
release occurred. Later independent archive coverage remains open.

## Bounded package/Desktop identity scans

The shared app-local inventory now enforces per-tree entry, byte, per-file, depth
and cooperative wall limits, opens through no-follow ancestor descriptors and
rechecks observed identities. It holds directory/file drift, path swaps and
special files instead of accepting incomplete payload identities. Package proof
receipts record default limits. The new control-only `test-artifact-inventory`
goal works without compiler/pkg-config selections.

Eight inventory, fourteen package-proof, twenty transaction and fourteen Desktop
preservation checks passed. Four newly identified controlled CLI capsules retain
expected success/failure with the final limits. The sealed
`20261007-package-inventory-bounds` packet verifies 26 files,
manifest SHA-256 `0fb9145192acc754213757ba2a04fe88dce68bf7f90ed0198cb895d8f9fe33bf`. Bounds are per tree and cooperative;
aggregate workflows, blocked syscalls, atomic filesystem snapshots and forced
supervisor death remain unqualified. No original cleanup, canonical adoption,
commit, package build, signing, installation or release occurred. Later archive
coverage remains open.

## Package assembly command supervision

Package transactions now use bounded retained command execution before staged
verification/publication. Default wall is 1800 seconds (maximum 3600) and combined
logs have a 64-MiB sampled cap. Separate stdout/stderr, partial staging, known exit
codes and terminal verification persist. Uncertain teardown blocks recovery and
reuse; no partial command failure publishes outputs. Existing verified publication
recovery remains available.

Twenty-four transaction, fourteen package proof and seventeen native contract
checks passed. Compiler-independent control targets passed; actual CLI SIGTERM
reaped its assembler and released package ownership. Four retained controlled
attempts verified success/reuse, failed exit, timeout and overflow. The sealed
`20261007-package-assembly-command-bounds` packet verifies 37 files,
manifest SHA-256 `bf95d3533b66c223c5aa97e542fc5f795e5a59e43c42cf2eb4d7d4a752546109`. Forced-death recovery, escaped
processes and aggregate workflow bounds remain open. No real package build,
signing, installation, release, commit, canonical adoption or original cleanup
occurred. Later independent archive coverage remains open.

## Retained release bundle audits and read-only contract

Release bundle audit now uses fresh retained proof capsules and exact output
admission. Framework reports are indexed per path, handling spaces and duplicate
basenames while preserving portable-link rejection. Existing package self-test
prerequisites remain. Release contract inspection no longer allocates directories
or enters compiler setup; profile-presence output correctly handles empty values.

Seven release-audit fixtures, fourteen package-proof and eight output-admission
regressions passed. A retained fake-package/tool fixture preserves legacy reports
across two successful audits and a framework rejection. Actual source contract
inspection passed with unavailable compiler/pkg-config and created no release
root. The sealed `20261007-release-audit-writers-final` packet verifies 82
files, manifest SHA-256 `ceb0f9125ca44899271a774774f7fd166da35c05431b8e979b4d1120388cf361`. Actual release tools,
signing/notarization/artifact writers, forced-death recovery, canonical adoption
and later archive coverage remain open. No real package build, signing, install,
release, original cleanup or commit occurred.

The initial retained audit fixture exposed a workspace-path false rejection from
scanning otool's audited-file header as a dependency. Exact source-file headers
(including architecture variants) are now excluded while dependency lines retain
the original forbidden-prefix checks. Raw reports are preserved. The initial
failed preparation is sealed separately in `20261007-release-audit-writers`; the
final fixture packet and seven audit regressions passed. This changes report
interpretation only; no native package or release acceptance is implied.

## Unsigned local artifact publication

Unsigned local ZIP/checksum/manifest output now uses retained package transactions.
Failure keeps staging and publishes nothing; exact completed input/output identity
permits no-write reuse. Checksums name the final ZIP basename. A bounded identity
verifier rejects incorrect checksums and malformed/corrupted unsigned manifests
before publication. Spaced roots and filenames remain supported.

Eight artifact, twenty-four transaction and seven audit checks passed. Retained
fake archive-tool success/reuse and partial failure demonstrate the actual recipe.
The sealed `20261007-local-artifact-transactions` packet verifies 135 files,
manifest SHA-256 `9ea8591b846a53f9c3e2c351a5524f1efd03bca678dbaff54809ba7dae69b36f`. Native ZIP contents/tool behavior,
signing/notarization and signed artifact writers remain unqualified. No real
release artifact, signing, installation, publication, commit, canonical adoption
or original cleanup occurred. Later independent archive coverage remains open.

## Aggregate package identity passes — October 7, 2026

Package input and output identity passes share an InventoryBudget across selected
roots. Entry counts, hashed bytes and elapsed deadlines cannot reset per tree;
missing input selections also count. Limits remain cooperative per identity pass,
not a hard quota or whole-workflow deadline. Combined output exhaustion retains
the failed attempt and refuses publication. See [current setup review](top_level_setup_review_20261007.md).

## Retained app-stage foundation — October 7, 2026

The [app-stage lifecycle](release_app_stage_lifecycle.md) adds separate signing
and stapling transactions with fake-tool proof, macOS metadata-preserving
staging/publication/recovery, and fail-closed recovery teardown records. Nested
command supervisors receive a bounded cooperative teardown window before forced
termination. Legacy signing/notary/export recipe cutover and real authentication
remain incomplete. No actual signing, notarization, installation or publication
was performed.

## Notarization journal foundation — October 7, 2026

[Retained notarization](release_notary_lifecycle.md) writes intent before submit,
keeps exact prepared-archive/signing bindings and reconciles only the retained
submission ID. Unknown submissions or unverified teardown hold; no blind
resubmission is provided. All SDK effect tests use fake tools. Archive producer
and full phase/Make cutover remain unfinished. No actual Apple service call ran.

## Notary archive and staple binding — October 7, 2026

[Archive production](release_notary_archive_lifecycle.md) now consumes exact
Developer ID signing evidence in a retained transaction. Stapling requires the
Accepted journal bound to that same app before allocation and rechecks it after
transformation. Portable sidecars, failed archives, same-app gates and predecessor
preservation have fake-tool proof. Full Make controller/export/refresh cutover,
ZIP content checks and real authentication remain incomplete.

## Bounded ZIP content gate — October 7, 2026

[ZIP payload verification](release_zip_validation.md) binds ordinary archived app
content, links and modes, preflights central metadata allocation and verifies
complete Deflate streams. Native fixture metadata structure is bounded and CRC
checked; complete xattr/ACL/resource-fork identity remains unqualified. Both
archive production and notarization admission enforce the gate. SDK effect
tools remain fake; full Make/export/refresh cutover remains open.

## Final artifact and bound refresh — October 7, 2026

[Final export](release_final_artifact_lifecycle.md) reproduces accepted/stapled
lineage, performs strict signature/stapler/Gatekeeper and architecture checks,
validates ZIP payloads, and publishes portable sidecars transactionally. Refresh
reproduces that completed artifact and selects its exact stapled app; temporary
Desktop tests retain predecessors. All authentication effects remain fake; full
Make entrypoint cutover and real qualification remain incomplete.
