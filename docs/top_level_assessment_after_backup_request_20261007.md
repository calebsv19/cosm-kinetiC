# PhysicsSim top-level operating assessment — 2026-10-07

PhysicsSim has substantial tested lifecycle hardening in Main Edit, but the
canonical checkout has not adopted it. Operational stability is therefore
improved in the development lane and still incomplete in the normal checkout.

## Backup and scope

The original 303 MiB CFD snapshot has independent PC archive and restore receipts.
The later frozen batch physics-lifecycle-hardening-20261007b contains 70 packets,
4,133 files, 45,872,267 logical bytes and an 8,001,051-byte archive. Local readback
and its three payload checksums pass; archive SHA-256 is
395fa4bc52e4d09e14fea400663001d38091806e47d7fd3914ad30d8e7b275d3.
The current copy attempt was rejected by automatic approval review because it
requires explicit approval of the exact payload and PC export/archive destination.
That precise question is pending. No upload began and originals remain retained.
Newer evidence, including this cache guard, is outside this frozen batch.

## Prioritized improvements

| Priority | Finding | Required stable behavior |
| --- | --- | --- |
| 1 | Canonical clean still uses broad recursive deletion over configurable roots; package/release recipes also delete recursively. | Adopt the bounded reviewed guard after exact ownership reconciliation and supported first proofs. Validate the complete plan before mutation; refuse unknown, changed, active or retained evidence. |
| 2 | Main Edit stages all seven slots and retains predecessors, but has retained rollback but lacks forward recovery and immutable-generation readers. | Stage and validate a complete generation, preserve the predecessor, serialize publication and support explicit recovery after interruption. The current path checks do not close this finding. |
| 3 | Retention classification exists, but terminal eligibility and guarded pruning are incomplete. | Separate disposable clean from evidence/environment/package retirement; bind each retirement to verified independent archive coverage and current checksums. |
| 4 | Failed/staged attempts are retained without a complete recovery and retirement lifecycle. | Read-only recovery plans, ownership and identity checks, explicit promotion or retirement, bounded worker lifetime/logs and truthful uncertain outcomes. |
| 5 | Build identity does not fully cover transitive headers, libraries, toolchain and configuration. | Reuse only when all effective inputs match; explain rebuild reasons and hold incomplete metadata. |
| 6 | Per-operation limits do not provide complete disk, workflow or I/O limits. | Aggregate budgets, explicit overflow behavior and supported deadlines with retained failure evidence. |
| 7 | Operator documentation contains extensive evolving chronology. | One concise guide for status/doctor, isolated build, headless proofs, clean preview/apply, archive/restore and retirement, with current adoption state. |

## Existing evidence and practical limits

Main Edit has tested whole-plan clean admission, isolated build identities,
compiler publication/ownership, retained fixture failures, reference environment
reuse, package/source transaction controls and bounded reference supervision.
The 194 identified CFD reference families use frozen anchored supervision; 175
factor compiler calls use retained transaction records. These are control-plane
proofs, not CFD numerical or real factor-ABI qualification.

Native settings/preset/preferences and 2D snapshots now use retained staged
publication. Configuration reads use bounded strict JSON/schema admission;
previously omitted settings and float precision loss have scoped repairs.
Startup runtime directory creation uses admitted paths. The latest scene-cache
path guard passes ten native fixture methods plus the isolated Make status
contract. A subsequent retained seven-slot transaction removes predecessor deletion and
holds incomplete publications, with sixteen native methods and the isolated
status contract passing. Digest-bound retained rollback now has twenty-five native/recovery methods.
Forward recovery, direct-reader consistency and
complete hostile path-race confinement remain open.

No commit, canonical adoption, package installation, release or pruning is
established by these proofs. Real platform/signing, installed and downstream
qualification remain separate. CFD accuracy remains a separate unmet gate.

The next implementation priority is canonical cleanup adoption after review,
followed by coherent cache publication and executable recovery/retirement.

Cache status now uses strict bounded metadata/schema and fixed-layout path
admission with thirty-five distinct native/status/recovery methods passing.
Retrospective proof fixes malformed JSON and external bundle acceptance. Complete
expected cache artifact inventories and immutable-generation consumers remain open.

Declared VF3D frame sequence, manifest/bundle agreement and raw header/payload
lengths are now admitted before publication, after staging and during status.
Forty-three distinct methods plus actual status/headless cache proofs pass.
Full payload/source authentication and immutable-generation consumers remain open.

Raw cache fields now stream under fixed buffers and finite-value checks; the mask
hash reuses the producer-owned contract. Fifty-two distinct methods and actual
status/headless proofs pass. Complete source/payload digest authentication, cached
readback/GUI responsiveness and immutable-generation consumers remain open.

Cache compiler-profile qualification now passes three methods / 258 actual
publication cases on Apple Clang 17 arm64, covering optimized, finite-math-only
and fast-math boundary compilation. No defect was demonstrated and production
code was unchanged. Other hosts, whole-program FP modes and non-cache boundaries
remain unqualified. See `cache_compiler_profiles.md`.

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
