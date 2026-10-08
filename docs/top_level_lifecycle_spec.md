# PhysicsSim top-level lifecycle specification

This is the PhysicsSim pilot contract for later CodeWork standardization. It
states required behavior; a link to an implementation or passing narrow test does
not establish repository-wide completion. Main Edit is the current implementation
lane. Canonical and installed/public surfaces require separate adoption evidence.

| ID | Required behavior | Owning evidence | Current coverage / remaining work |
| --- | --- | --- | --- |
| TL01 | Status and doctor inspect without installation, builds, generated-root allocation or network calls. Holds must be actionable. | `physics_status.py`, `physics_doctor.py`, their actual control-only Make tests | Implemented for current profiles; complete dependency provenance remains open. |
| TL02 | Root selection refuses protected storage, symlink components and overlapping lifecycle namespaces. | Path admission tests and real selected-root proof | Current helpers enforce this; local-session source/generated root, linked storage/lock/JSON and asset-path admission now covered; three native atmosphere run adapters now retain execution/acceptance attempts; native job JSON admission, known state/calendar timestamp admission, native identity/counter/summary consistency and per-file metadata publication now covered; remaining historical direct writers need migration. |
| TL03 | Cooperative writers own their full lifetime, with parent/child exclusion, sibling independence and child descriptor inheritance. | `build_owner.py` and behavior tests | Ordinary Make, migrated CLI lanes and atmosphere hash/execution/acceptance covered; inherited descriptor admission uses kernel witnesses and FD reaffirmation. Native public runner operations now span record reads through refresh/cancel/publication under a job guard. Detached native logs now use parent-admitted exclusive descriptors and checked child handoff. Fixture supervision now keeps an unreaped live group anchor through cleanup and uses a parent-only lifeline; tool/worker capture now also keeps a live unreaped anchor and propagates bounded terminal scope; file-backed retained-command supervision now also keeps a live unreaped anchor with nested TERM grace; atomic compiler supervision now keeps a live unreaped anchor, bounds shutdown and holds uncertain stages; outer Make supervision now keeps a live unreaped anchor, bounded nested shutdown and inherited build/jobserver descriptors; shared retained CFD run supervision now anchors cleanup and samples direct-command RSS under its unreaped waiter; 55 plain reference wrappers now use a frozen anchored helper with active -O admission and create-only receipts; 139 factor reference numerical-command loops are now also migrated; all 194 identified family loops use the frozen helper, and their 175 factor compiler calls now use retained staging and receipt-bound atomic records; full interrupted-prefix recovery and cached toolchain/config provenance remain open; remaining standalone writers, full descendant/process ownership and worker lifetime/log bounds require audit. |
| TL04 | Compiler publication preserves admitted predecessors on failure and refuses unknown/changed bytes. | `atomic_output.py`, exact ownership receipts, native/recipe tests | Ordinary compiler outputs and migrated contracts covered; historical recipes remain. |
| TL05 | Configuration changes rebuild; matching profiles are no-ops; interrupted or invalid metadata cannot reset identity history. | `build_identity.py`, actual Make generation/admission tests | Implemented for current ordinary graph; full tool/library dependency identity remains open. |
| TL06 | Clean validates the entire selected plan before mutation, then revalidates identities. Only exact disposable ownership permits deletion. | `clean_outputs.py`, protected-class tests, real retained-copy/clean proof | Current guarded clean covered in Main Edit; canonical uses older behavior. |
| TL07 | Reruns allocate fresh immutable evidence identities. Failure retains source, request, logs and terminal outcome when possible. Forced termination remains held. | Fixture, semantic, native, contract and package proof capsules | Migrated lanes, periodic JSONL, fresh mixed/native matrix series and bounded atmosphere convergence report capsules covered; local-session rolling sample history now preserves retired diagnostics and identities before active removal; fresh local-session author/validation attempts retain failed inputs/logs with bounded worker supervision; historical numerical writers and native job metadata preserves failed single-file publication; remaining operational jobs need audit. |
| TL08 | Reference setup reuses exact matching environments without writes and creates new profiles at their final prefixes. Existing incomplete/mismatched environments are held. | Reference setup lifecycle tests and full live inventory comparison | Current reference profiles covered; other tool families and full transitive identity remain open. |
| TL09 | Trusted local probes and supervised jobs have explicit resource limits and reap owned processes. Limits and gaps are stated accurately. | Tool probe, contract runtime and supervisor tests | Probe and retained contract/package proof/assembly command bounds implemented; retained logs use sampled bounds; per-tree Desktop and aggregate per-input/output-pass package inventory bounds implemented; native startup/error observation now has a ten-second deadline and bounded failure cleanup of the fresh unreaped direct child; aggregate workflow limits, forced-death recovery and remaining supervisors need audit. |
| TL10 | Package/refresh operations preserve predecessors, failed attempts and recovery state; source/package/installed/public claims remain distinct. | Package transaction, proof and desktop replacement tests | Current source helpers covered; fresh release bundle audits are covered; unsigned local artifacts are transactional; copy-on-transform signing/stapling and bounded ordinary ZIP payload validation, exact notarization journal/archive and acceptance-bound stapling and final artifact/bound refresh tested and receipt-bound release Make entrypoints now connected; downstream release consumer adoption, remaining writer audit and real-platform qualification remain open. |
| TL11 | Retention policy distinguishes scratch, disposable builds, evidence, environments, operational jobs and authenticated artifacts. Pruning requires explicit eligibility and verified archive coverage. | Versioned retention policy, read-only inventory, exact pruning plan and mutation readback | Versioned policy and bounded read-only inventory implemented; exact sealed-evidence coverage plans now bind copy/restore receipts and current/restored inventories; terminal ownership, class retirement eligibility and guarded pruning remain incomplete. No broad pruning is authorized by a passing clean test. |
| TL12 | Independent backup has exact payload coverage and demonstrated retrieval/readback. Later artifacts are not implied covered. | Archive completion receipt and empty-destination restore rehearsal | Prepared snapshot copied, independently retrieved and restore/readback rehearsed; later evidence coverage remains open. |
| TL13 | Adoption reconciles source ownership, dirty work, docs and supported first-start proof before canonical changes. Commit, install and publication decisions remain explicit. | Reviewed bounded adoption diff and source-checkout first proofs | Incomplete. No canonical adoption, commit or release has occurred. |

The supported first-start proof remains the headless build, deterministic Water
smoke and scene-project cache-output fixture from AGENTS.md. Ordinary lifecycle
checks do not certify CFD physical accuracy, historical campaign completeness,
package signatures, installation freshness or public release state.

A completion audit must inspect every supported writer and command family against
these requirements, including actual failure behavior. Retention and recovery
must be executable and verified rather than satisfied by prose alone. Future
program adoption should reuse the contract after PhysicsSim demonstrates it;
this document does not prescribe modifying other programs now.

Reference artifact completeness now has a scoped implementation: all 194 identified
wrappers admit the exact declared existing set, require complete successful output
and retain missing-output diagnostic failures. This strengthens TL07 and cached
readback; it does not close TL02 root namespace admission, TL09 aggregate limits,
TL11 retirement or TL13 canonical adoption. See `reference_artifact_inventory.md`.

Reference preparation now admits roots/retained entries and freezes bounded
regular sources create-only before compilation across all 194 identified families.
This advances TL02/TL07; complete namespace ownership, hostile path-swap
confinement, aggregate quotas, TL11 retirement and TL13 adoption remain incomplete.
See `reference_root_admission.md`.

Reference artifact hashing now has complete preflight and per-pass aggregate byte
admission with whole-pass identity checks across all 194 identified families.
This advances TL09; it does not establish I/O deadlines, disk/source/workflow
quotas or complete resource-policy qualification. See `reference_hash_budget.md`.

Reference wrappers now hold cooperative selected-parent namespace ownership and
inherit verified descriptors through frozen compiler/numerical supervision across
all 194 families. This advances TL03. Complete operational-job/process ownership,
covering outer-Make ancestor borrowing, recovery, TL11 retirement and TL13 adoption
remain incomplete. See `reference_namespace_ownership.md`.

Covering outer Make ownership can now be kernel-verified and borrowed by reference
runs while preserving its mode/lifetime. Separate reference hierarchy locks
serialize overlapping sibling recipes. Real parallel Make proof advances TL03;
complete operational-job ownership, recovery, retirement and adoption remain open.
See `reference_make_owner_borrowing.md`.

All reference final receipts now publish from retained admitted candidates by
atomic no-replace link under verified ownership, with exact readback and retained
failed attempts. This advances TL07. Coherent multi-file recovery, TL11 retirement
and TL13 canonical adoption remain incomplete. See
`reference_receipt_publication.md`.

Retained complete reference receipt candidates now have a read-only default plan
and explicit digest-bound owned no-replace recovery, with preserved failed
outcomes and exact idempotent readback. This advances TL07 recovery; interrupted
solver/compiler and coherent operational-job recovery, TL11 retirement and TL13
adoption remain incomplete. See `reference_receipt_recovery.md`.

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

## Retained scene-cache publication

Main Edit now stages all seven targets before mutation and retains displaced
predecessors instead of recursively deleting them. Sixteen native methods cover
copy failure, all fourteen rename boundaries and publisher death; the isolated
status contract passes. Pending attempts hold status/retries. Explicit recovery,
immutable-generation readers, retirement, full path/source provenance and
canonical adoption remain incomplete. See `cache_publish_transaction.md`.

## Cache publication recovery

Read-only digest-bound planning and explicit retained rollback now pass twenty-five
native/recovery methods, including all fourteen native publication rename boundaries,
interrupted rollback, relative journal portability and whole-plan drift. Restoration
retains new output and clears only the matching hold after verified restored content.
Forward candidate promotion, immutable-generation readers, archive-backed retirement,
hard I/O limits and canonical adoption remain open. See `cache_publication_recovery.md`.

## Cache status manifest admission

The status substring parser is replaced by reused strict bounded object/schema
admission and fixed active paths. Both manifests must agree, linked/special trees
hold, failed output clears readiness and owner identity is checked after reading.
Thirty-five distinct status/native/recovery methods pass. Retrospective proof
fixes prior ready acceptance for invalid trailing JSON and external bundle paths.
Expected frame/artifact completeness, immutable-generation direct readers, forward
recovery, archive-backed retirement and canonical adoption remain open. See
`cache_status_admission.md`.

## Declared cache artifact inventory

Exact VF3D sequence/manifest/bundle and raw header/length admission now runs before
source publication, after staging and on active status. Forty-three distinct
inventory/status/native/recovery methods and actual status/headless cache proofs
pass. Previous-reader comparison fixes ready acceptance with missing/truncated
frames. Full payload/source authentication, pack qualification, immutable-generation
readers, forward recovery, retirement and canonical adoption remain open. See
`cache_inventory_admission.md`.

## Streaming cache payload validation

Full raw field streams now reject nonfinite values and masks must match the
unchanged producer FNV hash. Fixed buffers, exact read loops, per-pass byte and
sampled time bounds have fifty-two distinct native/recovery methods plus actual
status/headless cache proof. Previous-reader comparison fixes ready acceptance
for nonfinite fields/damaged masks. Cryptographic source/payload authentication,
immutable-generation readers, forward recovery, hard I/O/aggregate quotas, GUI
responsiveness, retirement and canonical adoption remain open. See
`cache_payload_validation.md`.

Staged cache publication now compares every selected file in all four output
directories against its current source before predecessor displacement. A native
finite-value copy mutation demonstrated prior acceptance. Bounded complete-byte
comparison closes copy corruption while durable source authentication, initial
source binding, whole-plan immutability and canonical adoption remain open. See
`cache_copy_integrity.md`.

Cache publication now binds source semantic validation and the four staged
comparisons to an initial bounded in-memory source file/directory witness. Native
faults demonstrated prior acceptance of changed source bytes despite equal
staged copies. Ordinary source changes now hold before predecessor displacement;
durable authentication and whole-plan immutability remain open. See
`cache_source_witness.md`.

All four staged cache trees and three generated manifests now retain bounded
witnesses across validation and comparison, with whole-set and per-slot rechecks
before predecessor displacement. Prior late-staged mutation acceptance was
demonstrated. Cryptographic durable inventories, immutable-generation visibility
and generated-manifest intent readback remain open. See `cache_staged_witness.md`.

Generated cache manifests now pass strict complete-schema/request-intent
readback before publication; selection arithmetic rejects inconsistent or more
than 10,000 selected frames before index generation. Prior wrong-intent writer
acceptance was demonstrated. Authentication, immutable generations, forward
recovery and canonical adoption remain open. See `cache_manifest_intent.md`.

Headless output retirement now admits exact single-linked completed markers
and rechecks their identity after retained-slot allocation before moving a root.
Hardlink/noncanonical and three timing gaps were demonstrated against prior code.
Full worker ownership/log quotas, archived retirement and canonical adoption
remain open. See `headless_output_retirement_marker.md`.

Headless completion now retains its original running-marker witness and
requires that receipt before staging and promotion. Six prior changed-marker
completion acceptances were demonstrated; stage-time change retains the pending
receipt. Whole worker lifetime/log limits and terminal coherence remain open.
See `headless_output_completion_marker.md`.

Passed job summaries now require current strict completed-output evidence
before status/cancel can accept them; headless final console outcome follows
ownership checks. Four prior receipt-gap acceptances were demonstrated. Legacy
terminal records, failed/canceled coherence and process authentication remain
open. See `native_passed_summary_receipt.md`.

Completed states derived from saved records or final progress now require an
admitted summary before metadata publication. Counterproof showed label-only
success and a progress-only partial state write before later report failure.
Other multi-file terminal/coherence failures and authenticated process ownership
remain open. See `native_completed_summary_required.md`.

Shared job reports now build and validate with checked artifact strings before
status publication, holding demonstrated required-field failures without partial
status changes. This is preflight, not two-file atomic publication; I/O failure,
retained transaction/recovery and generation visibility remain open. See
`native_job_report_preflight.md`.

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

Existing report reads now require exact current schema, saved identity/state/
stage/timestamps and unique fixed artifact roles/paths before progress merge or
cancel. Counterproof demonstrated 21 prior mismatched-report acceptance cases;
98 native/recovery/runner methods and three integrations pass after repairing
independent fixture setup. Missing legacy reports, direct-reader generations,
worker ownership/resources, retirement and canonical adoption remain open.
See `native_job_report_coherence.md`. This advances the broad contract without
establishing full lifecycle completion or additional archive coverage.

All 58 direct native contract-fixture compiler recipes now reuse staged exact
ownership publication. Before-code probes demonstrated missing receipts and
unknown predecessor overwrite. Thirty-four lifecycle regression methods and the
final full native recipe family pass after six demonstrated link-source omissions
were repaired. Fresh-profile cleanup preview admits 720 owned files; the original
unclassified output remains held and preserved. See `contract_fixture_ownership.md`.
This advances writer coverage without closing provenance, retirement or adoption.

Disposable output ownership now requires single-linked regular files with stable
named/descriptor witnesses. Bounded cleanup metadata admits exact original-size
bytes and rechecks identity after decoding. Four prior admission failures are
closed; 112 focused and consumer methods pass, including linked toolchain
compatibility and existing owned-profile cleanup preview. See
`output_read_admission.md`. Complete hostile path-race confinement, budgets,
retirement and canonical adoption remain open; independent backup is unchanged.

Cleanup metadata now preflights depth 64 / 100,000 structural events and bounds
numeric tokens at 128 characters before conversion. Overflowing floats hold;
escaped strings and generic valid root types remain admitted. Six demonstrated
acceptance subcases are closed; 107 focused/consumer methods and owned-profile
cleanup preview pass. See `clean_json_bounds.md`. Aggregate memory/storage,
hard deadlines, retirement and canonical adoption remain incomplete.

Whole-tree cleanup inventory now preflights 100,000 entries / depth 64, 1 GiB
per file / 8 GiB total and 64 MiB metadata including ownership receipts before
hashing. Complete selected-tree and receipt witnesses are rechecked after hashes;
owned streams stop at original size plus one growth-check byte. Seven demonstrated
failing methods are repaired; 123 distinct focused/consumer methods and the
721-file owned-profile preview pass. See `clean_inventory_budget.md`. Sampled
120-second checks are not hard I/O deadlines. Canonical adoption, ancestor race
confinement, retirement and broader workflow budgets remain open.

Ancestor package reservations now have complete pre-read admission across at
most 64 ancestors: 1,000 single-linked regular JSON receipts / 64 MiB total,
with a 1 MiB per-file bound. Unknown entries hold. The original entire witness
set is rechecked after classification and again after output hashing, including
previously absent directories. Ten prior failing methods are repaired; 130
authoritative regression methods and the 721-file owned-profile preview pass.
See `clean_reservations.md`. Sampled time checks do not prove hard deadlines or
hostile race confinement. Canonical adoption, retirement and broader lifecycle
requirements remain open; independent backup coverage is unchanged.

Package reservation JSON now requires one of the two exact current producer
schemas, canonical UUID4 attempts, supported root namespace and canonical
contained output paths without linked components. Output strings bind SHA-256
reservation filenames; fresh roots and literal false authority must agree.
Twenty-three prior malformed acceptance subcases are repaired; 94 authoritative
methods and the 721-file owned-profile preview pass. Earlier reservation budget
fixtures now emit valid current records. See `clean_reservation_schema.md`.
Structural consistency does not authenticate producer execution. Canonical
adoption, retirement and the broader lifecycle requirements remain open.

Build configuration now binds selected implicit compiler environment values,
including include/library search paths and SDK/toolchain selections, preserving
absence, empty values and exact path order under bounded UTF-8 metadata admission.
Actual native Make counterproof showed three stale object/link cases; 36 distinct
focused/regression methods pass after repair, including matching no-op behavior.
See `build_environment_identity.md`. Selected directory/file contents, full
transitive provenance, wrapper toolchains, retirement and adoption remain open;
status does not claim complete profile identity verification.

Generated dependencies now have ownership/producer and restricted Clang grammar
admission before Make inclusion, with bounded count/file/aggregate reads and
complete file/receipt witnesses. Corrected counterproof demonstrated executable
Make expressions in both changed and ownership-recorded files, plus receipt/rule
gaps. Twenty-nine authoritative methods pass; read-only admission accepts all
330 dependencies in the retained native contract profile. See
`build_dependency_admission.md`. Hostile post-admission races and preserved-time
header-content provenance remain open alongside retirement and adoption.

Ordinary generated-dependency publication now records a bounded postcompiler
source/header content snapshot and binds object receipts to dependency SHA-256.
Graph admission forces only affected owning objects to rebuild on changed content
or legacy missing snapshots, then preserves matching no-ops. Actual Make proof
closes a preserved-timestamp stale header case; source isolation, legacy refresh,
contradiction and resource gates pass across 76 distinct methods. Read-only native
profile inspection reports 330 owned objects needing legacy refresh; none rebuilt.
See `build_input_content.md`. These observations do not establish immutable actual
compiler inputs; full system-header/library/SDK/toolchain provenance, coherent
publication, retirement and canonical adoption remain open.

Dependency-producing object compilation now has supervised dependency discovery
and matching bounded pre/post content plus full resolved-file identity observations.
Mutation or unreadable inputs hold publication and retain the candidate/hold receipt;
old accepted bytes/receipts remain unchanged. Three prior bad-publication cases are
repaired; 64 regression methods pass. A fresh real headless build, 233-output matching
no-op, 232 boundary-scoped dependency receipts, Water and scene-cache first proofs
and 467-file clean preview pass. See `compiler_input_boundary.md`. Observation
boundaries are not immutable inputs; full SDK/library/toolchain provenance, hostile
races, coherent publication, retirement and canonical adoption remain open.

A single declared Clang object set now drives dependency admission/inclusion and
configuration prerequisites. Read-only real graph inspection closes six omissions:
three headless stubs and three shape tools; all 340 objects have deduplicated deps.
Eight before-code subcases fail; 33 regression methods and final coverage tests
pass. Fresh native build / 233-output matching no-op / all 232 built dependencies
admitted / Water / scene-cache / 467-file cleanup preview pass. See
`object_dependency_coverage.md`. Direct C/link inputs, non-dependency/FisiCs and
conditional/platform provenance, retirement and adoption remain open.

Native final-link counterproof now establishes two stale reuse cases: selected
archive content changes with preserved mtime, and a previously absent archive
appears in an earlier unchanged search path. Fresh links return the updated
values; current Make reuses the old executable. Installed Darwin dependency traces
identify selected inputs and unsuccessful searches, including relative misses.
See `linker_input_selection.md`. The repair must bind both content and absence
witnesses; it is not implemented or qualified by these discovery experiments.
Canonical adoption, retirement and full lifecycle completion remain open.

Selected Darwin object-only links now bind executable receipts to admitted linker
selection manifests. Before/after link observations cover selected contents and
absent search candidates; mutation holds accepted bytes and retains candidates.
Graph reuse relinks changed inputs and refreshes unbound legacy outputs once,
while contradictory metadata holds. Both earlier stale cases are repaired.
Seventy-three distinct methods pass, alongside final real headless build,
234-output unchanged repeat, Water/cache proofs and 469-file clean preview.
See `link_input_identity.md`. Direct-source/platform/toolchain completeness,
coherent publication/recovery, retirement and canonical adoption remain open.

Shape mask/asset links now reuse the declared timer_hud_external cJSON object and
its source/header closure, removing their raw C link input. Both Darwin final links
join linker manifest reuse admission. Three before-code countertests fail and 33
current methods pass; fresh native tool builds, 12-output matching no-op, real
U-shape conversion byte parity/source preservation and 22-file clean preview pass.
The real default 340-object/dependency graph remains complete and deduplicated.
See `shape_link_objects.md`. Other direct-source lanes, platform/toolchain identity,
retirement and canonical adoption remain open.

Pack/dataset/trace direct C compiler commands now use tool-private owned objects,
preserving their legacy compile prefixes and binding effective tool flags to build
identity. All 29 added objects have generated dependency and Darwin final-link
admission; the real default graph has 369 deduplicated objects/dependencies.
Nine before-code fixture subcases fail; 42 current methods pass. Fresh real tools,
35-output matching no-op, three native export/parity fixtures and 66-file clean
preview pass. Optional private pack-inspector injection avoids shared build writes
in this proof; its default path remains a separate lifecycle concern. See
`cli_object_identity.md`. Emitter/contract/platform closure, retirement, full
provenance/recovery and canonical adoption remain open.

Emitter diagnostics now uses tracked private source/header objects and Darwin
linker manifests, with effective flags bound to configuration. Its previously
broken native link is repaired by including the existing complete scene-compiler
family and mesh-path resolver; the existing Darwin bundle declaration rule covers
its new bundle object. Dot-component target names normalize consistently. Forty-one
distinct methods, final real native build, a successful retained-scene diagnostic
step/source preservation, 106-output no-op and 213-file clean preview pass. Default
declarations have 473 deduplicated objects/dependencies. See
`emitter_object_identity.md`. Full platform/toolchain/resource/recovery qualification,
retirement, contract/worker closure and canonical adoption remain open.

Emitter index parsing now admits bounded decimal/range values before scene/config
work, replacing atoi coercion. Eight real native methods pass, including FIFO
pre-open refusal and preserved default/zero behavior. A regression also exposed
whole-second/newer-binary stale links after object rebuild; explicit dirty-object
dependent links and new-configuration link forcing repair two deterministic
countercases. Fifty distinct final methods, native build, 106-output matching no-op
and 214-file clean preview pass. Failed/intermediate gates remain retained. See
`emitter_argument_admission.md`. Full lifecycle/resource/recovery/provenance,
contract/platform closure, retirement and canonical adoption remain open.

Shape mask/asset numeric conversion now rejects overflow/range/nonfinite values
before input I/O. Mask allocation is capped at 64 MiB and explicit option domains
are admitted. A real old-binary wrapping grid conversion and two FIFO pre-open
failures are retained; seven current native methods and three shape Make/link
regressions pass. Both fresh tools build, normal conversion bytes match unchanged
baselines, matching repeat preserves twelve outputs and clean preview admits
22 files without cleanup. See `shape_numeric_admission.md`. Extreme finite
raster arithmetic, geometry complexity, direct publication, full resource limits,
retirement, canonical adoption and independent newer backup remain open.

Native shape raster preflight now bounds geometry/curve sample conversion,
transformed integer coordinates, stroke and whole-pass raster work before caller
mask mutation. Twenty-five distinct final methods pass (15 sanitizer, seven CLI,
three Make/link), both fresh tools build, five valid line/curve cases preserve
baseline bytes, matching repeat preserves twelve outputs and clean preview admits
22 files without deletion. A terminal old-source NaN probe incorrectly succeeds;
current code refuses without mutation. Two earlier sanitizer attempts remain in
observed UE state after termination requests, including a corrected invalid polygon
fixture; they are held and excluded from acceptance. See `shape_raster_admission.md`.
Native source/asset consumers, publication, process holds, full limits, retirement,
backup and canonical adoption remain incomplete.

Shape mask PGM output now reuses admitted retained persistence and reports write
failure with exit 1, replacing direct truncation/unchecked close. Current source
inode aliases and protected output paths refuse. Real old-binary file-size fault
reproduces predecessor truncation; current code preserves it and retains a failed
candidate. Forty-three distinct methods pass; fresh tools build, successful output
bytes match unchanged baselines, fourteen outputs remain unchanged on repeat and
26-file clean preview passes without cleanup. Mask-specific persistence dependency
propagation has an actual Make proof. See `shape_mask_publication.md`. Asset/input
admission, recovery/retirement, earlier sanitizer holds, complete limits, backup
and canonical adoption remain open.

Shape input now reuses bounded strict native JSON reads and an additive borrowed-
text decoder in the local unversioned vendored shape candidate. Canonical shared
source/header remain checksum-unchanged. Consumed schema/case/coordinate admission,
whole-document counts and geometry checks protect mask/asset loading without a
filename reopen. Sixty-five distinct final methods pass; five mask and one asset
parity proofs, fifteen-output repeat and 28-file clean preview pass. Earlier
sanitizer holds remain observed UE; current gates are terminal. See
`shape_input_admission.md`. Asset output, full limits/recovery/retirement,
upstream/canonical adoption and independent newer backup remain incomplete.


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
