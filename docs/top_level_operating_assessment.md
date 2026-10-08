# PhysicsSim operating assessment — October 7, 2026

The prepared backup is copied and has demonstrated independent retrieval and
restore readback. Main Edit's build and cleanup controls are stronger, but the
ordinary canonical checkout still has broad recursive cleanup. The full lifecycle
pilot is incomplete. No canonical adoption, commit, installation or release has
occurred. This is the current overview; the dated setup review retains the
implementation chronology and the lifecycle specification tracks all thirteen
requirements.

## Backup coverage

The existing PC archive holds the prepared 303 MiB snapshot at:
`/mnt/cold_archive_500g/codework-archive/generated-runs/physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a`.

The copy matched six payload checksums. Independent retrieval/restore matched
6,391 historical files and 560 fresh files, including eleven sealed bundles.
The copy and restore receipt bundles were reverified for this assessment.
Originals and intake remain intact. Later hardening packets are outside this
snapshot. A separate seventy-packet snapshot has passed local restore readback
and is prepared for the existing archive; its transfer awaits explicit approval
after automatic review rejected scope beyond the earlier prepared backup. See
`later_lifecycle_backup_preparation.md`. Missing pre-incident evidence remains missing. Backup preservation
does not qualify numerical accuracy.

## Priorities for a reliable top level

| Priority | Improvement | Current state and completion criterion |
| --- | --- | --- |
| 1 | Put the guarded cleanup into ordinary use | Main Edit validates the whole selected plan, protects unknown/retained content and permits only exact owned disposable output. Canonical still has `rm -rf` recipes. A bounded seven-file interim guard candidate has passed build, Water and scene-cache proof; adoption remains pending. Reconcile dirty CFD work and verify the exact adopted files before claiming protection in canonical. |
| 2 | Finish output ownership across commands | Classify every supported native, Python and shell writer by root, owner, output class, overwrite and failure policy. Native headless overwrite now retains its predecessor; summary/progress publication is atomic. Many migrated tests and atmosphere/session attempts retain diagnostics. The Python AST inventory alone is not a complete writer audit. |
| 3 | Finish supervised-job persistence and recovery | Job paths and JSON reads are admitted and bounded. Selected request/status/report/PID/cancel publication is now atomic per file with competing-writer holds. Public runner read/update coordination is now guarded. Log startup now uses parent-admitted descriptors and bounded exec/error channel observation. Lifetime/log bounds, multi-file recovery and authenticated process ownership remain incomplete. Add verified interruption recovery and prevent concurrent metadata writers from silently replacing state. |
| 4 | Make retirement and backup coverage executable | Artifact classes and read-only retention audit exist. Exact retirement eligibility, terminal ownership, checksum-bound archive coverage and guarded pruning remain incomplete. Archive later evidence as a new identified batch and verify retrieval before considering deletion. |
| 5 | Complete setup and build provenance | Isolated roots, configuration fingerprints, atomic compiler publication and exact reference-environment reuse are covered in current profiles. Full transitive headers/libraries/tool identity and other environment families need qualification. Incomplete profiles remain held rather than reset. |
| 6 | Finish package/release consumer integration | Source transaction and receipt-bound release helpers preserve predecessors and failed attempts. Controlled tests do not qualify real signing, Apple acceptance or installed packages. Downstream consumers and platform qualification remain separate gates. |
| 7 | Establish one stable operator contract | Keep read-only status/doctor, isolated build, supported first proofs, cleanup preview/apply and archive/restore as explicit steps. Publish the final canonical guide after adoption and completion; use PhysicsSim as a template for other programs only after those checks. |

## Current operating guidance

Preserve the held original build tree. Use Main Edit with a distinct admitted build
profile for development. Preview its cleanup plan before applying cleanup; a
retained/unknown-content hold needs owner reconciliation. Ordinary clean must
never imply evidence pruning, environment deletion or package retirement.

The canonical checkout was inspected clean at
`3fa1ad5f6fb2384a7626e83c5e22fa7821cb7ed5`. Its broad deletion still appears in
runtime, release and macOS/Linux package recipes. Main Edit remains uncommitted
at base `dd55d7c0f4bbf6f7f4e614e5ee71ded7982b1962`, alongside pre-existing CFD work.
Do not copy the entire drift into canonical as a cleanup fix.

The latest native field admission and live-observation slice passed 120 affected checks and six integrated
CLI, Water, scene-cache and detached-runner fixtures. Earlier slices have their
own sealed proof packets; counts are not aggregated into a repository-wide
acceptance claim. Resource limits are cooperative and have stated gaps.
Numerical CFD gates, installed-product freshness, public versions and the full
thirteen-requirement lifecycle completion remain separate.

See `top_level_lifecycle_spec.md`, `top_level_setup_review_20261007.md`,
`native_job_json_lifecycle.md` and
`data/experiments/lifecycle-validation/20261007-native-job-json-admission`.

## Subsequent state and timestamp admission

Persisted job states now reject unknown labels, and consumed timestamps require
exact UTC syntax and valid calendar dates before refresh/cancel. Seventy-four
affected native-job checks and six integrated fixtures passed. See
`native_job_state_time_admission.md`. Full required schemas, timestamp ordering,
counter consistency and authenticated worker lifetime remain incomplete.
The later archive upload was rejected again by automatic approval review;
the exact-payload/destination authorization question is pending. No upload began.

## Subsequent identity and counter consistency

Present native status/progress identities and effective counter relationships are
now checked before refresh/cancel publication; progress cannot change requested
work or output identity. Temporary-record merge preserves the caller on holds.
Seventy-eight affected checks and six integration fixtures passed. See
`native_job_consistency.md`. Required schemas, authenticated request/worker
identity, summary consistency and transition history remain open. Canonical and
the pending archive are unchanged.

## Subsequent summary observation and terminal consistency

Refresh now reuses one admitted summary, checks its identity/work/outcome, holds
contradictory terminal records and prevents terminal progress regression. Matching
success summaries can recover completed counters when final progress is missing.
Eighty-two affected checks and six integration fixtures passed. See
`native_job_summary_consistency.md`. Full schemas, authenticated worker lifetime
and multi-file recovery remain open. Canonical and the pending archive are unchanged.

## Exact evidence archive-coverage planning

A read-only planner now verifies sealed copy/restore receipt binding and exact
current/restored file coverage for a selected evidence bundle. Twenty affected
checks and the eight-check control-only Make entrypoint passed. The real older
20261007-b2 bundle matched 144 files; the newer summary packet correctly remained
uncovered. Coverage remains held until class-owner terminal/retirement decisions
and guarded pruning are implemented. See `evidence_retirement_plan.md`.

## Fixture cleanup process identity

Fixture supervision now retains a live, unreaped process-group anchor through
cleanup and observes parent loss through a parent-only lifeline. Receipts separate
direct-child reaping and cleanup requests from unverified complete descendant
termination. Thirty-three affected checks and six integration fixtures passed.
Similar ordering in tool probes and retained-command supervision remains open.
See `fixture_process_group_ownership.md`. No retirement or canonical adoption occurred.

## Tool/worker capture cleanup identity

Bounded capture now retains a live unreaped group anchor through cleanup, with
separate result and parent-lifeline channels. Report/atmosphere receipts propagate
its direct-child/command scope and complete-descendant hold. Forty-nine affected
checks and all six integration fixtures passed. File-backed retained command
supervision remains the next related migration. See
`tool_capture_process_ownership.md`. Canonical and archive scope are unchanged.

## Retained-command cleanup identity

File-backed retained commands now keep live unreaped group identity through
cleanup, preserve a nested TERM grace window and react to parent-lifeline loss.
Eighty-four affected checks and a real 231-file sealed native contract passed.
Session/contract/package receipts separate limited terminal scope from complete
descendant lifetime. See `retained_command_process_ownership.md`. Full process
audit, retirement/pruning and canonical adoption remain incomplete.

## Compiler group anchoring and held staging

Atomic compiler publication now retains live unreaped group identity through
shutdown, bounds compiler observation/interruption and holds staging when terminal
ownership is uncertain. Twenty-nine affected checks passed, including escaped
compiler inherited-lock exclusion and actual compiler/evidence contracts. See
`compiler_process_ownership.md`. The outer Make owner and older numerical-run
supervisors still require migration; complete descendant termination, retirement
and canonical adoption remain incomplete. The later prepared backup remains local
after automatic approval review rejected transfer scope; no upload started.

## Whole-Make group anchoring

The outer Make owner now keeps a live unreaped group anchor, bounds observation
and interruption, handles parent loss and preserves inherited build/jobserver
descriptors through both hops. Thirty-one affected checks passed, including real
parallel Make and unknown-output cleanup holds. See `make_process_ownership.md`.
Complete descendants, older numerical-run supervisors, retirement and canonical
adoption remain open. The earlier compiler section's remaining outer-Make
migration is now implemented for this documented scope.

## Shared CFD run helper ownership

The retained CFD helper now owns an unreaped group anchor and samples direct
command RSS under its only waiter. Wall/log observation is bounded, failed
compiler candidates remain retained and receipts state descendant limits.
Twenty-five affected checks and the eight-check control-only Make route passed.
See `cfd_run_process_ownership.md`. Independently implemented older numerical
supervisors, complete descendant ownership, retirement and canonical adoption
remain open; this did not rerun a numerical campaign.

## Plain reference wrapper migration

Fifty-five plain reference wrappers now run their frozen anchored supervisor,
retain combined logs, enforce admission under -O and create new receipts without
replacement. Solver commands/environments/RSS and wall caps were compared and
remain unchanged. Fresh and cached synthetic control cases passed across all
55 families; cached zero-exit diagnostic rejection now stays rejected. See
`reference_wrapper_supervision.md`. The remaining 139 factor-building wrappers,
strict legacy root/JSON admission, retirement and canonical adoption remain open.
No numerical campaign or physical accuracy gate was rerun.

## Factor reference command supervision

The remaining 139 factor reference numerical commands now use their frozen
anchored supervisor. All 194 identified family polling loops are migrated, with
solver/factor commands, environments and numerical-command caps compared unchanged.
The six-method control suite passed, including 278 fresh factor-family synthetic
cases and cached readbacks; plain-family Make regression also passed. See
`factor_reference_supervision.md`. Direct factor compiler/record transactions,
strict legacy admission, retirement and canonical adoption remain open. Synthetic
factor fixtures do not qualify native library ABI or CFD numerical accuracy.

## Factor compiler and bound build records

All 175 direct factor compiler calls in 139 wrappers now use bounded retained
staging, and compiler/SDK identity probes retain bounded output. Factor records
publish atomically without replacement and bind exact compile receipt/library
bytes for reuse. The final family suite, six focused transaction checks and five
retained-compiler checks passed; a sealed native control library persists its
load/install-identity and unchanged record readback. See
`factor_build_transactions.md`. Actual CFD factor ABI, toolchain/config identity
on reuse, interrupted-prefix recovery, retirement and canonical adoption remain
open. No numerical campaign or installed-product acceptance is implied.

## Current review and reference-record admission

See `top_level_current_review_20261007.md` for the prioritized operating review.
All 194 reference families now use bounded retained JSON/progress reads and
run-scoped nofollow artifact hashes. Nineteen distinct scoped methods passed;
declared command assignments compared unchanged across all families. The sealed
398-file `20261007-reference-record-admission` packet retains sources, baseline,
audit and logs. Exact expected artifact inventories, root completion, aggregate
budgets, retirement and canonical adoption remain open. The user's latest copy
request was rejected again by automatic review for exact later-payload/destination
authorization; the explicit question is pending and no upload began.

## Required reference artifact inventories

All 194 wrappers now supply their expected artifact paths to cached admission.
Omitted existing artifacts and unexpected entries hold; zero-exit fresh runs with
missing required output retain a diagnostic failure and stay rejected on cache
readback. Failed partial runs remain readable with exact present-file accounting.
Twenty-one distinct affected methods and an actual missing-snapshot wrapper control
passed. The initial consistency cached-path initialization regression was repaired
and the full suite rerun. See `reference_artifact_inventory.md`. Root namespace
admission, aggregate budgets, retirement and canonical adoption remain incomplete.

## Reference preparation path and source admission

All 194 reference wrappers now admit selected roots, namespace entries and
retained source copies before generated allocation. Checkout source hashing and
create-only freezing are bounded, regular and nofollow with byte/identity witnesses.
Six duplicate static dependency manifests were deduplicated without changing
ordered unique sources or declared commands. Thirty-six distinct affected methods
passed, including 388 all-family linked-root/source cases. See
`reference_root_admission.md`. Namespace locks, hostile concurrent path-swap
confinement, aggregate quotas, retirement and canonical adoption remain open.

## Aggregate reference artifact read budgets

Fresh/cached artifact hashing across all 194 wrappers now preflights the complete
pass under 8 GiB per-file and 16 GiB aggregate logical-byte limits before artifact
content reads. Admitted-size streaming and whole-pass witnesses hold growth,
changed earlier files and artifact-presence drift. Thirty-nine distinct affected
methods passed, including sparse-over-budget no-read behavior and whole-family
regression. See `reference_hash_budget.md`. I/O deadlines, disk/workflow quotas,
namespace locks, recovery, retirement and canonical adoption remain open.

## Cooperative reference namespace ownership

All 194 reference public runs now hold namespace ownership across preparation,
compiler/tool/numerical execution, artifact reads and receipt publication/readback.
The adapter follows existing build/clean lock hierarchy and verifies inherited
FDs through named-inode and kernel witnesses. Compiler/numerical children inherit
ownership; closing parent copies does not unlock inherited child descriptions.
Fifty-eight distinct affected methods passed, including seven kernel-ownership
methods, full-family regression and existing build-owner tests. See
`reference_namespace_ownership.md`. Complete descendant lifetime, covering outer
Make-ancestor borrowing, operational-job ownership, recovery, retirement and
canonical adoption remain open.

## Covering Make ownership borrowing

Reference runs now borrow a kernel-verified covering Make/build owner without
converting its selected exclusive lock or closing borrowed descriptors. A separate
reference intention/exclusive hierarchy excludes competing equal/overlapping
recipes within the same Make and permits unrelated siblings. Sixty-three distinct
affected methods passed, including five focused borrowing methods and real
parallel Make recipe handoff. See `reference_make_owner_borrowing.md`. This closes
the previous covering-Make borrowing gap for this tested cooperative scope;
operational-job ownership, full descendant lifetime, recovery, retirement and
canonical adoption remain incomplete.

## Transactional reference receipt publication

All 194 wrappers now retain request/candidate bytes and publish admitted complete
receipts by atomic no-replace link under verified namespace ownership. Candidates
and directories are synced; collisions preserve predecessors; failed attempts
remain retained. Seventy distinct affected methods passed, including seven
publication behaviors and full-family regression. See
`reference_receipt_publication.md`. Single-record publication does not close
coherent interrupted-run recovery, full descendant lifetime, retirement or
canonical adoption.

## Explicit retained receipt recovery

A read-only default operator adapter now plans exact retained receipt candidates
under existing lock exclusion without generated allocation. Explicit digest-bound
apply rechecks source/artifact/terminal/request identity and publishes no-replace;
matching readback is idempotent and failed outcomes remain failed. Forty distinct
affected methods passed, including nine recovery methods. The 194 wrapper/helper
bytes match the prior full-family publication proof. See
`reference_receipt_recovery.md`. Interrupted solvers/compiler prefixes, coherent
operational-job recovery, retirement and canonical adoption remain open.

## Cross-language writer discovery

A bounded read-only inventory now covers selected Python, native C/header, shell
and Make sources: 1,750 files, 5,357 static signals and 418 explicit script edges,
with five focused tests passing and no selected admission gaps. Behavioral writer
coverage remains unverified. Concrete predecessor-loss candidates include preset,
preference and snapshot saves and destructive scene-cache overwrite publication.
See `writer_discovery.md` for inspected triggers and repair priorities. This
advances audit discovery, not TL02/TL03/TL07/TL11/TL13 completion. Canonical
and independent-backup coverage are unchanged.

## Native preset and preference save repair

Main Edit preset and theme/font save entrypoints now reuse admitted staged
publication with cooperative parent ownership, predecessor preservation and
retained failed candidates. Fourteen focused native methods, seventeen existing
atomic regressions and four affected Make contracts pass. Runtime directory
creation refuses links; invalid preset bounds/serialized strings hold before
staging. False after rename can mean unconfirmed complete visible bytes. See
`native_persistence.md`. Text/snapshot writers, scene-cache replacement, GUI
save-result handling, aggregate quotas, recovery/retirement and canonical
adoption remain incomplete. No shared API/version or backup coverage changed.

## Runtime configuration save migration

Configuration save now uses admitted retained atomic publication; normal and
legacy-headless shutdown consume preset/config save results and report
unconfirmed saves with failure exit status. Interactive shutdown requests an
error dialog. Six native configuration tests and fourteen persistence regressions
pass, as does the isolated configuration Make contract and main source compilation.
GUI/runtime shutdown failure handling remains unqualified. Older config
reader/JSON escaping and linked-ancestor startup directory creation remain open
findings, alongside text/snapshot/cache writers, retirement and adoption. See
`config_save_lifecycle.md`. No shared version or backup coverage changed.

## Startup runtime directory admission

The earlier linked-ancestor startup finding now has a scoped repair: the fixed
data/runtime/scenes and snapshot graph is admitted before allocation and created
through nofollow directory descriptors with witness checks and sync. Ten native
methods, twenty configuration/persistence regressions and two isolated consumer
contracts pass. Partial creation is retained; full hostile path-swap confinement
and whole-operation ownership remain unqualified. See
`runtime_directory_lifecycle.md`. Config JSON admission/escaping, other writers,
retirement and canonical adoption remain open. Backup coverage is unchanged.

## Configuration file read admission

Configuration reads now admit nofollow bounded regular single-link input, verify
exact size/EOF/current identity and reject empty/NUL-containing or changed files.
Missing fallback follows full path syntax admission. Eight native methods, twenty
save/persistence regressions and an isolated Make roundtrip pass. Source config
reads remain supported. Full JSON/schema/range/escaping semantics and hard I/O
deadlines remain open. See `config_read_admission.md`. This advances input
admission without closing broader lifecycle, retirement or adoption requirements.

## Configuration JSON contract migration

The substring parser is replaced by reused strict bounded object parsing and
representation admission for all 59 consumed fields. Saved strings escape and
roundtrip correctly; staged candidates validate before publication. Forty
configuration/persistence methods, twenty existing job parser/publication
regressions, an actual Make roundtrip and both current default-config readbacks
pass. Physical ranges/relations, full option persistence, I/O deadlines and
installed qualification remain open. See `config_json_contract.md`. Broader
writer/recovery/retirement/canonical adoption requirements remain incomplete.

## Configuration option and numeric persistence

The nine previously omitted readable options now save, and float/double output
uses roundtrip-safe significant digits. Four native option methods plus forty
existing regressions and an actual isolated Make roundtrip pass. The retained
previous writer fails both omission and precision probes that current code passes.
All 52 consumed configuration members are exercised; fields outside this file
contract, physical ranges/relations, locale success and installed qualification
remain open. See `config_option_persistence.md`. Broader lifecycle/adoption work
and independent backup coverage remain incomplete.

## Native 2D snapshot publication

Snapshots now use admitted retained staging under explicit class bounds, check
shape/time and exact binary byte count, and preserve predecessors on tested
prepublication failures. Seven snapshot plus thirty-one persistence/configuration
methods and an actual isolated backend contract pass. Retrospective proof shows
identical successful format bytes and fixes partial-write predecessor destruction.
See `snapshot_publication.md`. Hard storage/I/O limits, snapshot recovery/retirement,
coherent scene-cache replacement and canonical adoption remain incomplete.

Existing shared reports now require bounded strict object admission before refresh
or persistence, including unchanged refresh. Actual-runner probes demonstrated
malformed regular report rejection after status mutation and successful repeat
status with the malformed report still present; linked/special path controls
already held. Forty-five runner methods and three integrations pass. Full staged
pair publication, retained recovery and cancellation coherence remain open.
See `native_job_report_predecessor.md`.

Native metadata now separates checked preparation from single-file publication,
retaining locks and complete stage witnesses through the decision. Seventeen
metadata methods plus 45 runner methods and three integrations pass; a two-stage
rejection preserves both originals. This is an additive bridge. Status/report
still need coordinated staging, a durable hold/plan, retained predecessors and
explicit recovery; pair atomicity remains unqualified. See
`native_job_file_preparation.md`.

Production status/report persistence now fully stages and admits both files,
retaining both metadata locks and whole-pair witnesses before first publication.
A prior actual-runner report-lock contention failure changed status and left an
old report on retry; the new regression preserves both and later publishes
coherent outcomes. Sixty-three native/runner methods and three integrations pass.
Sequential promotion still needs a durable hold/plan, retained originals,
interruption/recovery and generation visibility. See `native_job_pair_staging.md`.

Fixed status/report publication now retains bounded old/new byte copies and a
physical-root-bound plan before creating a durable pending hold. New runner
operations refuse held jobs. Both forced-death promotion boundaries, second-rename
EIO and foreign hold mutation retain evidence; successful target readback clears
only the owned hold. Seventy native/runner methods and three integrations pass.
Explicit digest-bound recovery, full durable-boundary faults, direct-reader
visibility, storage retirement and canonical adoption remain open. See
`native_job_pair_hold.md`.

Held job pairs now have strict read-only SHA/witness-bound planning and explicit
exact-plan retained rollback. Native interruption at both publication boundaries,
all four predecessor presence combinations, six SIGKILL rollback rename boundaries,
partial stages and drift/source-storage gates are covered. Eighty-two distinct
native/recovery/runner methods and three integrations pass. Snapshot hashes bind
observed evidence, not historical producer authentication. Forward recovery,
full durable-boundary faults, generation readers, retirement and canonical adoption
remain open. See `native_job_pair_recovery.md`.

New job-pair v2 publications store exact retained-role SHA-256 digests and bind
the journal in the pending hold using the existing shared one-shot byte utility.
Recovery rejects v2 content/journal mismatches before first planning; legacy v1
is explicitly marked without producer inventory evidence. Eighty-five native /
recovery/runner methods and three integrations pass, including a 1 MiB digest
oracle and oversize pre-attempt hold. Unsigned receipts do not authenticate
producer identity. Forward recovery, reader generations, full faults/retirement
and canonical adoption remain open. See `native_job_pair_producer_digests.md`.

Held v2 job pairs now permit explicit exact-plan forward installation alongside
retained rollback. Durable intents bind one recovery direction; opposing choices
hold. Legacy v1 cannot forward. Native pre/after-promotion states, all presence
combinations and six SIGKILL forward rename boundaries are covered. Ninety-five
native/recovery/runner methods and three integrations pass. Reader generations,
worker execution/outcome provenance, full faults, retirement and canonical
adoption remain open. See `native_job_pair_forward.md`.


Shape asset publication now reuses retained host persistence and an additive local
shared text serializer. The converter's generated default is data/runtime rather
than source config/objects. Seventy-five distinct methods pass, including all 25
reached serializer allocation faults, normal byte parity and both dependent links.
Legacy shared path-only saves still truncate directly. This packet is outside the
frozen seventy-packet backup; canonical and archive coverage are unchanged. See
shape_asset_publication.md. Complete lifecycle qualification remains open.


Editor import conversion now shares the qualified CLI publication helper, writes
outside source assets by default, rejects return/model path truncation and stops
trusting existing filenames as validated cache. Sixty-four distinct methods pass;
normal tool bytes match baseline, matching repeat preserves 18 files and clean
preview admits 35 files. Surrounding picker failure handling and in-memory asset
freshness remain open. See editor_import_publication.md. Canonical, independent
archive coverage and full lifecycle completion are unchanged.


Picker and canvas-drop add now share candidate/commit behavior, preserving the
working scene and prior cache on failure and refreshing reimports in stable slots.
A full scene/missing library refuses before conversion; invalid path/count/position
and bounded library growth have native coverage. Fifty-six distinct methods pass;
normal bytes match baseline and matching repeat preserves 19 files. Shared asset
file loading remains blocking/unbounded and is the next related migration. See
editor_picker_transaction.md. Canonical and independent archive coverage remain
unchanged; complete lifecycle qualification is still open.


Picker asset input now reuses bounded strict host JSON admission and an additive
local shared text decoder. Seventy-five methods pass, including real FIFO/linked
picker refusal, shared legacy compatibility and both dependent links. Normal bytes
match baseline; repeat preserves 20 outputs and clean preview admits 38 files.
Startup library discovery and writer/reader limit reconciliation remain next.
See shape_asset_input_admission.md. Canonical and archive coverage are unchanged;
full lifecycle qualification remains incomplete.


Asset publication now shares the host reader's structural/schema/text contract,
refusing unreadable output before staging/replacement. Ninety-nine distinct methods
pass, including aggregate counts and encoded-name boundaries. Normal bytes match
baseline; matching repeat preserves 20 files and clean preview admits 38. Startup
legacy directory loading remains the next migration. See
shape_asset_roundtrip_policy.md. Canonical/archive coverage and full lifecycle
completion are unchanged.


Startup asset loading now uses bounded deterministic host inventory and the admitted
asset reader. Selected failures never partially publish; file/directory changes,
aggregate budgets and operational identities have native proof. Forty-seven methods
pass, and all eight checked-in geometries match legacy decoding by name. Repeat
preserves 19 files and clean preview admits 36. See
shape_library_input_admission.md. Full desktop/installed proof, generation identity,
raster safety, recovery/retirement, canonical adoption and archive coverage remain
open; complete lifecycle qualification is not claimed.


Runtime PhysicsObject asset rasterization now admits geometry, float transforms,
integer endpoints, mask allocation and aggregate segment/polygon work before
allocation or output publication. Sixty-one distinct methods pass, with the 27
builder methods also passing targeted ASan/UBSan instrumentation and all eight
checked-in asset masks retaining parity. A compiler identity transition correctly
rebuilt; the matching final repeat preserves 18 outputs and clean preview admits
35. See physics_object_raster_admission.md. The 2D caller still needs mask_count /
grid-capacity admission, and broader consumers/recovery/retirement/canonical/
archive qualification remain open. Complete lifecycle qualification is not claimed.


The runtime 2D import-mask caller now admits exact grid capacity and input metadata
before touching output, stages raw fallback masks and preserves caller bytes on
failure. Forty-nine distinct methods and both existing 2D Make contracts pass;
22 import-mask methods also pass targeted sanitizers. Matching repeat preserves
18 outputs and clean preview admits 34 files. See import_mask_admission.md.
One additional historical sanitizer countertest remains held in UE state with
its unreaped parent, separately from terminal current gates; no reaping or terminal
diagnostic is claimed. Aggregate backend allocation/partial-failure policy,
remaining consumers/recovery/retirement/canonical/archive work remain incomplete.


2D backend construction now admits a 256 MiB initial requested-storage budget
before allocation and refuses every partial required-field allocation via complete
candidate destruction. Forty-eight distinct methods and both existing 2D Make
contracts pass; all 15 construction allocation faults release tracked ownership.
Twenty-six construction methods also pass targeted sanitizers. Matching repeat
preserves 18 outputs and clean preview admits 34 files. See
backend_storage_admission.md. Later emitter/scratch storage, runtime configuration /
state dimensions, standalone fluid allocation callers and global/recovery/
retirement/canonical/archive qualification remain open. The earlier historical
sanitizer child remains held separately; no reaping is claimed.


2D runtime storage now retains its allocation dimensions and refuses mismatched
scene/config/fluid extents before mutation, emitter replacement or misleading
views. Constructor admission/allocation uses one selected dimension snapshot.
Sixty-seven distinct methods and both existing 2D contracts pass; identity and
construction suites also pass targeted sanitizers. Matching repeat preserves 19
outputs and clean preview admits 36 files. See grid_identity_admission.md.
Numeric/inventory admission, later emitter/scratch ownership, global/recovery/
retirement/canonical/archive qualification remain open. Prior process holds remain
separate; full lifecycle completion is not claimed.


The 2D brush now clamps coordinates before integer conversion and admits finite
input/target additions before changing any of density/velocity. Thirty-eight
distinct methods and both existing 2D contracts pass; brush cases also pass
targeted sanitizers. Matching repeat preserves 18 outputs and clean preview admits
34 files. See brush_admission.md. Motion/emitter numeric and inventory boundaries,
later storage lifetimes, recovery/retirement/canonical/archive qualification remain
open. Full lifecycle completion and prior-process reaping are not claimed.


Object-motion injection now stages a bounded whole pass, clamps world coordinates
before integer rounding and checks original-order per-cell velocity additions
before any mutation. Invalid later input or allocation/overflow failure preserves
all fluid fields. Sixty-six distinct methods and both existing 2D contracts pass;
28 motion methods also pass targeted sanitizers. Matching repeat preserves 18
outputs; clean preview admits 34 files without deletion. See motion_admission.md.
Other inventory/emitter boundaries, later storage lifetimes, recovery, retirement,
canonical adoption and newer archive coverage remain open. No full lifecycle or
historical-process reaping claim is made.


## Entrypoint scope correction and frame deletion hold

Writer discovery now includes tools and bounded extensionless shell/Python
entrypoints plus Make helper-variable bindings. The current scan selects 1,839
files, 6,308 static signals and 746 edges with no selected admission gaps; ten
previously omitted entrypoints are now visible. Legacy rm_frames now previews
retention for its own checkout and refuses blind deletion. Twenty-two distinct
tests and real control-only Make repeats pass without a compilation profile.
See entrypoint_cleanup_audit.md for the newly identified Wind probe recursive
reset, bundler temporary cleanup and packaged-launcher/installer publication gaps.
Behavioral completeness, guarded retirement, canonical adoption and newer backup
remain open; the expanded inventory is discovery, not acceptance.


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
