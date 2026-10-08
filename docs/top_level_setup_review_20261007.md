# PhysicsSim setup and cleanup review — October 7, 2026

The prepared backup copy is complete and verified. PhysicsSim's Main Edit has
substantially stronger lifecycle controls, but the canonical checkout still uses
the old cleanup and package recipes. Adoption is the highest priority for making
the repairs effective in ordinary use. No canonical sync or commit occurred.

## Backup and recovery

The existing PC archive holds the prepared 303 MiB snapshot at:
`/mnt/cold_archive_500g/codework-archive/generated-runs/physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a`.
The copy receipt records matching payload checksums and preservation of originals.
Independent retrieval and restoration matched 6,391 historical files and 560 fresh
files, including 11 sealed evidence bundles. Both receipt bundles were reverified
in this review. Later hardening evidence is outside this snapshot. Restore
integrity does not establish solver accuracy or recover missing earlier evidence.

## Current state

| Area | Main Edit coverage | Remaining work |
| --- | --- | --- |
| Top-level clean | Exact ownership and identity checks, whole-plan admission, protected storage and unknown evidence held, cooperative exclusion | Canonical still recursively removes the build root and selected binaries; adopt the guarded controls before ordinary cleanup |
| Build setup | Isolated selected roots, configuration fingerprints, atomic output publication, compiler/dependency failure preservation | Full transitive input identity and forced-death recovery; classify legacy direct writers |
| Environments | Read-only doctor/status, pinned reference profiles, exact matching reuse without reset; incomplete profiles held | Cover other tool families and full dependency provenance |
| Tests and reports | Fresh retained attempts, bounded logs, frozen source inputs, failure preservation in migrated lanes | Complete the writer inventory, including historical numerical campaigns and operational jobs |
| Package lifecycle | Predecessor-preserving transactions and recovery checks; fresh audit reports; unsigned local artifact staging | Copy-on-transform signing/stapling, exact notary archive/journal, bounded ZIP verification and acceptance-bound final export/refresh tested; receipt-bound release Make cutover implemented; downstream consumer adoption and real platform qualification remain open |
| Resource limits | Input and output identity passes now share entry, byte and deadline budgets across all selected trees | These are cooperative per-pass limits; whole-workflow, forced-death and hard resource guarantees remain open |
| Retention | Artifact classes and bounded read-only audits; unknown content stays held | Exact retirement eligibility, archive-coverage matching and guarded pruning are incomplete |
| Operator contract | Explicit lifecycle specification and supported first-start commands | Consolidate accumulated progress documents into a concise current operating guide |

Canonical was freshly checked clean at `3fa1ad5f6fb2384a7626e83c5e22fa7821cb7ed5`.
The implementation remains uncommitted in Main Edit, based at
`dd55d7c0f4bbf6f7f4e614e5ee71ded7982b1962`, alongside pre-existing CFD work.
The canonical Make recipes still contain broad deletion in top-level clean,
release cleanup and desktop/worker package operations. The Main Edit top-level
clean delegates to guarded inventory admission. Do not infer that a source fix
has changed the installed application or public release.

## Recommended work order

1. Prepare a bounded adoption diff for the cleanup, root selection, ownership and
   setup foundation. Reconcile pre-existing CFD work and run the supported
   headless build, Water smoke and scene-project cache-output proof on that exact
   candidate before adopting it in canonical.
2. Finish a command/output ownership inventory. Every supported writer needs a
   declared output class, root, owner, overwrite policy, failure record and cleanup
   rule. Treat historical output as held until its owner classifies it.
3. Complete retirement and backup coverage. Archive later evidence as a new batch;
   make retirement plans bind exact checksums, terminal ownership and demonstrated
   restoration. Keep destructive cleanup separate from ordinary build cleanup.
4. Finish the release lifecycle under its owning workflow. Preserve predecessors
   through signing/export/refresh and prove interrupted-operation recovery. Source
   fixture tests cannot substitute for macOS/Linux package qualification.
5. Publish one current operator guide: prerequisite check, isolated build, supported
   proof, cleanup preview/apply, archive/restore and recovery. Use the proven
   PhysicsSim contract as a template only after adoption and completion checks.

## Latest release-source assessment

The final-artifact and bound-refresh slice passed 48 targeted checks: seven final
artifact checks, fourteen Desktop preservation checks and twenty-seven package
transaction checks. A control-only Make invocation also passed without compiler
or package prerequisites. Its sealed evidence packet was reverified with 19
files and manifest SHA-256
`4358a3119b07adca4cbca35731e23ff62afe030a6660e50fbcb4c341ec38b505`.

The source helpers reject mismatched bundle product/version, wrong architecture,
nonzero Gatekeeper assessment, invalid ZIP payloads and changed acceptance
lineage. Final refresh selects the app bound to the completed artifact receipt
and preserves the predecessor. Authentication tools were fake and refresh used a
temporary Desktop. No Apple submission or real Desktop modification occurred.

The Main Edit signing/notarization/stapling/artifact/refresh Make entrypoints now
use the receipt-bound pipeline. Source apps and phase predecessors are preserved;
Gatekeeper failures stop final export. Pending acceptance stops downstream work,
and reruns query the same retained submission ID without another submit. The
unsigned local artifact contract remains separate. Authenticated outputs use
`RELEASE_PIPELINE_ROOT/final` and exact receipts; downstream consumer adoption
and real-platform qualification remain open. See `release_pipeline_lifecycle.md`.

Evidence: `data/experiments/lifecycle-validation/20261007-final-artifact-and-bound-refresh`.
The prepared backup does not cover this newer packet.

## Review validation and limits

108 local regression checks passed across artifact inventory (11), package
transactions (26), package proof (14), desktop preservation (14), unsigned local
artifacts (8), release audits (7), guarded clean (15) and doctor (13).
Combined-input budget exhaustion is tested before package-root creation; combined
output exhaustion retains the failed stage and publishes no final output. Shared
entry counts and deadlines cannot reset at each tree. Normal identity data remains
unchanged. Package/release fixtures use controlled fake tools; no real package,
signing, installation or publication occurred. No survivor deletion occurred.

These checks validate lifecycle behavior, not CFD force/transient accuracy,
installed-product freshness or repository-wide completion. Whole-workflow hard
resource limits and uncooperative filesystem-call deadlines are not claimed.
The original build evidence remains held. The full thirteen-requirement pilot
specification remains incomplete.

Evidence: `data/experiments/lifecycle-validation/20261007-package-aggregate-inventory`.
Earlier assessment: `top_level_stability_assessment_20261007.md`.
Specification: `top_level_lifecycle_spec.md`.

## Release Make cutover follow-through

The receipt-bound Main Edit Make pipeline passed six end-to-end/control-only
checks and seven final-artifact regression checks after native `macOS` and
stem-sidecar alignment. No old in-place signing/notary/staple loops or ignored
Gatekeeper errors remain in Main Edit's release recipes. Signed outputs are
separate from unsigned preparation paths. Existing public release consumers
require exact final receipt/artifact staging before adoption; no such staging,
public release, canonical sync or real Desktop refresh occurred.

Evidence: `data/experiments/lifecycle-validation/20261007-release-make-cutover`.
This evidence is newer than the independently stored prepared snapshot.

## Operational sample cleanup repair

The local session service previously deleted rolling sample identities/results
and expired pending requests solely by age. Main Edit now copies retired samples
into verified operational history, flushes the retained namespace before active
removal, and preserves request IDs for matching retries. Pending requests stay
held. Terminal results include retained histories. Ten focused checks and nine
existing real-worker session checks passed. Other operational writers/root
admission and full run retirement remain separate audit work.

See `session_sample_retention.md` and sealed evidence
`data/experiments/lifecycle-validation/20261007-session-sample-preservation`.
This packet is newer than the prepared independent backup.

## Session storage admission follow-through

The service previously resolved supplied roots before creation and used ordinary
linked-path JSON/lock opens. Main Edit now rejects source/protected/broad roots,
linked storage and special locks/JSON before writes, checks scene asset paths and
uses bounded, identity-checked JSON reads. Duplicate/nonfinite metadata and
nesting above 128 are held before decoding. Fourteen focused checks and nineteen
operational regressions passed. These are cooperative trusted-local controls;
complete root ownership, native-worker admission, scratch retention and process
recovery remain open. See `session_path_admission.md`.

Evidence: `data/experiments/lifecycle-validation/20261007-session-path-admission`.
It is outside the prepared independent backup and has not been adopted in canonical.

## Retained authoring and validation attempts

Scene authoring/validation no longer remove temporary inputs on failure. Fresh
operational attempts retain request/stage/log/receipt data and bind worker bytes.
New scene reuse requires a completed author receipt. Shared bounded supervision
now covers these commands and is included in contract proof control provenance.
Operational/session/evidence classes override disposable claims in guarded clean.
Ten focused attempt checks, fifty affected regressions and sixteen clean checks
passed. No original evidence, canonical source or installed package was changed.
Full executable dependency trust, root ownership, forced-death reconciliation and
exact attempt retirement remain open; see `session_worker_attempts.md`.

Evidence: `data/experiments/lifecycle-validation/20261007-retained-session-attempts`.
Newer evidence remains outside the prepared independent backup.

## Reviewable canonical cleanup preservation candidate

A seven-file interim guard was prepared against canonical `3fa1ad5f6fb2384a7626e83c5e22fa7821cb7ed5`,
excluding unrelated Main Edit drift. Eight guard cases and all three supported
first proofs passed. Independent patch application reproduced the tested bytes.
Canonical remains clean/unmodified. The guard holds outputs from existing legacy
writers; full build ownership adoption remains open. Adoption authority is pending.
Review: `data/experiments/lifecycle-validation/20261007-canonical-clean-guard-candidate/REVIEW.md`.

## Attempt input capture follow-through

Retained attempts now preserve admitted worker/control bytes, bind control hashes
and worker permissions, detect import-time and mid-attempt source drift, and
verify retained copies before completion. Successful selected-worker command
evidence is mandatory; missing evidence stays held. Seventeen focused checks and
thirty-three operational regressions passed. Loaded bytecode/interpreter/native
libraries remain outside complete dependency proof; no authentication is claimed.
Evidence: `data/experiments/lifecycle-validation/20261007-session-attempt-input-capture`.
Canonical cleanup adoption remains pending the earlier user approval question.


## Session storage identity witnesses

Each client now pins four non-inherited descriptors for its root, scenes, runs
and service lock. Public operations check those identities before and after
execution; locks compare the acquired file with the pinned lock. Authoring and
validation also check storage before completing retained attempts. Validation
uses the existing service lock. Ordinary directory or regular lock replacement
is held, including replacement during an operation. The original incomplete
receipt remains held rather than writing a terminal receipt into a replacement
root. Constructor admission creates the empty service lock when absent.

Client close releases the four descriptors and prevents reuse; it does not stop
a native worker. These are cooperative checks, not descriptor-relative writes
or protection against transient uncooperative races. Individual scene/run
ownership, native worker storage and forced-death reconciliation remain open.

Twenty path checks, eighteen retained-attempt checks and nineteen operational
regressions passed (57 total). Evidence:
`data/experiments/lifecycle-validation/20261007-session-storage-witnesses`.
This packet remains outside the prepared independent backup and canonical.


## Atmosphere adapter failure retention

The read-only Python candidate inventory covered 774 files and 2,487 mutation
sites without parse errors; native/Make/indirect writers remain separate. Three
confirmed temporary-directory preservation gaps were migrated: passive, evolving
and open-atmosphere adapters now retain fresh worker/input/log/acceptance
capsules, with unchanged numerical gates. Twenty-two focused/lifecycle and
twenty-one adapter regression checks passed. Full dependency identity, hard
workflow limits, forced-death recovery and exact retirement remain open.
See `atmosphere_attempt_lifecycle.md`. New evidence remains outside the backup
and canonical adoption.


## Native headless overwrite preservation

Native audit found recursive deletion under `--overwrite`. Main Edit now retains
an admitted completed predecessor in a fresh sibling slot, exclusively claims
new output and holds linked/protected/unknown/incomplete roots. Thirty targeted
checks, an integrated source build and all three CLI/Water/scene-project proofs
passed. Actual preserved two-frame/replacement one-frame output was read back.
Explicit sidecar paths, detached-runner metadata and forced-death recovery remain
open. See `native_headless_output_lifecycle.md`. Canonical is unchanged.


## Native sidecar admission

Summary/progress now preflight together before output effects, exclusively claim
fresh files and bind file/parent descriptors through updates. Operational classes
protect them from clean. Forty-two targeted checks and six final CLI/runner
fixtures passed. Mutable sidecar atomic publication and detached-runner metadata
still require repair. See `native_headless_sidecar_lifecycle.md`. Canonical and
independent archive remain unchanged.


## Atomic native sidecar generations

Native summary/progress updates now stage, flush, validate and atomically publish
a generation rather than truncating the live file. Failure preserves the readable
predecessor and pending diagnostics; the client holds further completion. Forty-
eight targeted checks and six final integrated fixtures passed. Forced-death
reconciliation, multi-file consistency, complete payload qualification and
detached-runner metadata remain open. See `native_headless_sidecar_atomic.md`.
Canonical and independent backup remain unchanged.


## Detached native job path admission

Jobs roots, local IDs, constructed slot lengths and fixed regular metadata
entries are admitted before effects. Status identity and sidecar/log references
must match the selected slot. Requests/status now carry operational classification.
Fifty-five affected checks and six integrated fixtures passed. Native status
refreshes metadata; signal-zero liveness is not authenticated process ownership.
Strict bounded parsing, atomic metadata, serialization and recovery remain open.
See `native_job_path_admission.md`. Canonical and independent backup are unchanged.


## Bounded native job metadata

Request, bundle, status, progress and summary reads now admit bounded regular
files and strict JSON objects, including duplicate decoded keys, nonfinite values,
UTF-8, nesting and input identity checks. Final affected checks passed 67 tests
and all six integrated fixtures passed. An initial status formatting regression
was fixed and retained in the evidence. Atomic native job metadata, lifetime
serialization, process identity and recovery remain open. See
`native_job_json_lifecycle.md` and the concise `top_level_operating_assessment.md`.
Canonical and independent backup are unchanged.


## Native job metadata publication

Canonical request, status, shared envelope/report, PID and cancellation updates
now stage, validate and atomically publish single-file generations. Failed or
interrupted stages retain the predecessor; new destinations remain absent until
complete publication. Per-parent competing-writer locks and predecessor drift
witnesses hold unsafe updates. Clean and retention audit protect operational
locks and pending diagnostics even against mistaken disposable claims. Eighty
affected checks and six integrated fixtures passed. Job-wide coordination,
multi-file consistency, append logs and process/recovery identity remain open.
See `native_job_metadata_publication.md`. Canonical and backup are unchanged.


## Native runner operation ownership

Submission now exclusively creates its slot; status and cancellation hold the
job-level guard from before their record read through refresh and publication.
Root/parent/lock witnesses hold later effects after namespace replacement.
Refresh failures stop cancellation, descriptor release permits independent
siblings and subsequent operations, and guard state remains protected from clean.
Eighty-nine affected checks and all six final integrated fixtures passed.
Worker lifetime/log ownership, multi-file consistency, process authentication and
forced-death recovery remain open. See `native_job_operation_guard.md`.
Canonical and prepared backup remain unchanged.


## Native detached log handoff

Stdout/stderr are now preflighted and exclusively created in the guarded parent
before fork; the child checks the root/file witnesses and redirects only the
admitted descriptors. Closed standard streams are handled without descriptor
aliasing. Existing/linked/special/replaced destinations hold; failures preserve
allocated logs. Ninety-eight affected checks and six integrated fixtures passed.
Log byte limits, worker lifetime ownership, startup acknowledgment and recovery
remain open. See `native_job_log_handoff.md`. Canonical and backup are unchanged.


## Native detached startup observation

Submission now observes a close-on-exec startup/error channel with a ten-second
deadline rather than equating fork with launch. Setup/exec failures and timeouts
retain job/PID/phase diagnostics, with bounded cleanup of only the fresh unreaped
direct child. Unconfirmed cleanup stays held without a claimed finish. One hundred
and six affected checks and all six integrated fixtures passed. This does not
authenticate the executable or prove application readiness, descendant cleanup or
worker lifetime ownership. See `native_job_startup_observation.md`.
Canonical and prepared backup remain unchanged.


## Later lifecycle archive preparation

Seventy later sealed packets (4,133 regular files) are selected into a separate
7.6 MiB compressed archive. All source bundles were reverified, and an empty
local restore matched every checksum and all bundle manifests. The supported
export-dropbox item is staged locally. Automatic approval review rejected upload
because the earlier authorization covered the original specifically prepared
backup; this later payload needs explicit authorization. No upload, independent
copy, deletion or canonical adoption occurred. See
`later_lifecycle_backup_preparation.md`.


## Native known-field admission and live observation

Request/status/progress now reject present field coercions, integer overflow and
string truncation. Invalid metadata holds refresh instead of being ignored.
Initial sidecar reservations stay distinct from published metadata. Live atomic
generation drift permits only bounded read re-observation, without mutation or
worker retry. One hundred twenty affected checks and six final integrated fixtures
passed; the earlier failed cancellation fixture was cooperatively canceled and
its summary read back. Full schemas, consistency and process/lifetime ownership
remain open. See `native_job_field_admission.md`. Canonical and the approval-gated
seventy-packet archive are unchanged; this newer packet is outside that scope.

## Native status vocabulary and timestamp calendar admission

Status refresh/cancel now holds unknown persisted states and malformed consumed
UTC timestamps before publication. Exact digit/range/calendar checks replace loose
scanf plus time normalization. Four new behavior cases, 74 affected native-job
checks and six integration fixtures passed. No canonical or archive changes were
made. See `native_job_state_time_admission.md`; required schemas, ordering, counter
consistency and worker ownership remain open.

## Native identity and counter consistency

Status/progress declarations now bind provided identity tags and requested-work
relationships before refresh/cancel publication. Invalid merges leave the caller
unchanged. Producer boundary cancellation and zero terminal steps are covered.
Seventy-eight affected native checks and six integration fixtures passed; the
stall fixture's formerly inconsistent identities were corrected without dropping
its assertions. See `native_job_consistency.md`. Full required schemas, summary
consistency, transition history and authenticated process lifetime remain open.

## Native summary and terminal consistency

One admitted summary observation now drives refresh. Provided identity/work/result
fields and terminal outcomes must agree; terminal progress cannot regress state.
Matching success summaries recover completed counters without final progress.
Eighty-two affected checks and six integration fixtures passed. Full schemas,
worker authentication and multi-file recovery remain open. See
`native_job_summary_consistency.md`. Canonical and archive scope are unchanged.

## Checksum-bound evidence retirement planning

Read-only plans now connect sealed independent copy and restore receipt chains
to exact current/restored inventories. Twenty affected checks, eight control-only
Make checks, a 144-file real coverage readback and an uncovered later-packet hold
passed. Terminal/class-owner eligibility and guarded pruning remain open.
No deletion or archive transfer occurred. See `evidence_retirement_plan.md`.

## Fixture group anchor and terminal receipt precision

A live unreaped direct child now anchors group identity until cleanup. Bounded
result and parent-lifeline pipes preserve fixture outcome and parent-loss cleanup.
Terminal receipts no longer equate group signaling with complete group reaping.
Thirty-three affected checks and all six integrated fixtures passed. macOS zombie
group visibility/refusal and post-kill lookup races were resolved without weaker
ownership admission. Related probe/retained-command supervisors remain open.
See `fixture_process_group_ownership.md`; canonical and archive scope are unchanged.

## Tool capture live ownership

Tool/worker capture now signals before reaping its live direct anchor, retaining
output/time/descriptor bounds. Lost owners and invalid result channels hold.
Report/atmosphere receipts state bounded terminal scope instead of implying all
descendants are proven terminal. Fifteen focused and 49 affected checks plus six
integration fixtures passed. File-backed retained command supervision remains
open. See `tool_capture_process_ownership.md`. No canonical/archive changes occurred.

## File-backed retained command anchor

Retained-command supervision now signals before reaping its live anchor, with
bounded result/lifeline channels and five-second nested TERM grace. Tags/bounds
hold before logs/launch. Eighty-four affected checks and a real fresh native
obstacle-box contract passed; its 231-file capsule verified. Isolated Make
fixtures now stage their existing owned-command dependency. Receipts explicitly
hold complete descendant qualification. See `retained_command_process_ownership.md`.
Full process/retirement audits and canonical adoption remain open.
