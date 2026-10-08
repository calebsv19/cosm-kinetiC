# Retained local package transactions

The source/Main Edit Linux worker and desktop package targets now wrap assembly
in `scripts/package_transaction.py`. Existing host/build prerequisites remain in
place. This adds filesystem lifetime and exact-reuse behavior; it does not grant
release, signing, installation, remote submission or Registry authority.

A transaction owns the selected release root cooperatively and participates in
checkout cleanup exclusion. Each fresh attempt keeps a journal, assembly log and
stage under `<release-root>/.package-transactions/<attempt-id>/`. Exact final
paths are reserved before assembly. Every effective package output path is mapped
into the stage for recursive Make. Build-owner and GNU Make jobserver descriptors
are forwarded. The original stage remains retained after publication.

The assembly identity binds declared package metadata, selected payload files
and directories, Make/scripts/config/docs and packaging source, assembly command,
selected interpreter/direct tool binaries, and relevant locale/archive/Python
path environment values. Directory inventories include bytes, sizes, modes,
internal links and empty directories. Missing optional inputs are recorded as
absent. Inputs are checked again after assembly, during reuse readback and after
publication. This is conservative exact input identity, not a proof that every
ambient library or undeclared external input is captured.

After assembly succeeds, declared outputs are inventoried, copied to publication
staging and checked against the retained stage. Native exclusive renames publish
one output at a time, with journal updates and final whole-set readback. Files or
directories already occupying a final name are never intentionally reset. Worker
checksum sidecars now use the archive basename, remaining valid after staging
publication. Existing package format and artifact naming are preserved.

A completed transaction can be reused only when its full selection, input identity
and final output inventory match. Reuse does not relaunch assembly or rewrite
artifact bytes. Input/tool/environment drift, missing/tampered outputs and existing
unbound artifacts hold reuse; a fresh release-controlled root is required for
changed artifacts. The completed marker proves assembly storage/readback only.
Native package validation, package self-test, authentication and installed/public
readback remain distinct required gates.

Failed commands retain logs, staging, reservations and any already published
outputs. Multiple-output publication is not one atomic swap. Interrupted/partial
attempts hold ordinary retry; exact-attempt recovery can now complete previously
verified staging as documented below. This helper does not remove predecessors or authorize
pruning. Normal clean holds retained transaction evidence. Power-loss behavior
and native Linux exclusive-rename execution remain unqualified on this Mac.

Twelve disposable transaction checks cover exact repeated reuse, tool/input/archive
environment drift, drift during assembly/readback, failed commands, partial
publication retention, cleanup and concurrent package exclusion, unsafe path
mappings and unknown predecessor preservation. The actual worker outer/internal
Make recipes run with fake binaries, temporary source inputs and stubbed
host/build prerequisites; recursive -j and portable sidecars pass. This is recipe
and filesystem evidence, not a native worker package qualification. The Linux
desktop assembly recipe is source-inspected only. Eight admission, nine cleanup
and five whole-Make ownership checks also pass for this slice.


## Exact partial-publication recovery

From the owning source checkout, select an exact retained attempt for a read-only
plan:

```bash
python3 -B scripts/package_transaction.py --recover-attempt "<release-root>/.package-transactions/<attempt-id>"
```

An operator authorized to recover those local outputs can add `--apply` after
reviewing the selected output root and missing names. Recovery revalidates the
plan under cleanup/package exclusion, checks the original contract identity,
exact reservation ownership, retained stage inventory, current input identity
and every existing published output. It copies only missing outputs to fresh
recovery staging and publishes through exclusive rename. It does not reassemble,
replace occupied names, delete prior outputs or change release authority.

Recovery requires an original recorded verified-stage inventory. An assembly
that failed before reaching that point remains held and needs a fresh controlled
root; its partial stage is not accepted merely because files happen to exist.
Changed source/tool/environment, altered contract or reservations, tampered stage
or published payloads, unsafe paths and active owners hold recovery.

The original attempt journal remains unchanged. Separate receipts under
`<attempt>/recovery/<recovery-id>/` bind its checksum, input identity and final
inventory. Failed recovery attempts retain their payload and journal. A later
recovery determines missing outputs from exact readback, including a rename
completed before its checkpoint was written. A completed recovery becomes a
completion proof for ordinary verified reuse; repeated recovery with complete
matching output returns already_completed.

Twenty transaction tests now cover actual SIGKILL before the first publication,
after the first and after the last, plus SIGKILL during recovery itself. They also
cover CLI plan/apply, idempotent recovery and subsequent reuse, failed copy/retry,
occupied names, changed reservations/contracts/inputs/stage/published bytes and
cleanup/package lock exclusion. Eight admission, nine cleanup and five Make
ownership checks pass. These are local Mac storage/process and fake-payload
recipe proofs. Native Linux execution, package semantic validation, authentication,
installed acceptance, machine power-loss durability and proof-root lifetime remain
separate unqualified work.

## Bounded assembly commands and teardown gates

Assembly children now reuse the retained command runner. Default wall time is
1800 seconds, with --wall-cap selecting at most 3600. Combined stdout/stderr uses
a 64-MiB sampled limit, reducible with --log-cap. Exclusive assembly.stdout and
assembly.stderr replace the new attempt's combined log; historical assembly.log
files remain retained. Partial staging and logs survive failed exit, timeout,
overflow and catchable interruption. Publication begins only after command
success, verified teardown, staged-output inventory and input rechecks.

Receipts record limits, terminal_processes_verified and known assembly exit codes.
Uncertain signalling/wait records terminal verification false; recovery and reuse
hold that transaction. The failed attempt and reservations remain intact. Existing
verified partial-publication recovery is preserved. A forced kill before verified
assembly leaves the initial false terminal state held; this does not implement a
forced-death recovery or descendant reconciliation policy. Escaped descendants,
blocked syscalls, aggregate inventory/copy time and hard disk quotas remain open.

`make test-package-transaction test-package-proof` uses control-only routing.
Twenty-four transaction, fourteen proof and seventeen native contract checks pass,
including actual CLI SIGTERM child reaping/package-lock release and the existing
fake-worker Make recipe. Four retained controlled CLI attempts demonstrate success
with exact no-write reuse, failed exit, timeout and log overflow. Failed attempts
publish nothing. Evidence is sealed in
`data/experiments/lifecycle-validation/20261007-package-assembly-command-bounds`.
These are fixtures; real platform/package, signer, install and release acceptance
remain unqualified. No survivor cleanup or canonical adoption occurred. Later
independent archive coverage remains open.

## Unsigned local release artifact transactions

`release-local-artifact` keeps its bundle-audit prerequisite and now transacts the
ZIP, checksum and manifest together through fresh retained staging. Exact output
reservations and no-replace publication preserve unknown/completed predecessors;
failed archive or metadata generation leaves staging/logs without final publication.
Matching inputs reuse the verified transaction without rewriting final outputs.
Archive/shasum tools and app/Make/helper inputs participate in selection identity.

The checksum records the ZIP basename, allowing readback after publication and
relocation. Shell dirname/basename handling preserves spaced output roots and
filenames. Before completion, the identity verifier checks bounded inventories,
exact ZIP/checksum agreement, unique manifest fields, artifact basename/hash,
format declaration and false signing/notarization fields. It does not qualify ZIP
contents, native archive behavior or authentication. Real package verification
remains with the owning release workflow. Direct inner assembly refuses protected
or occupied outputs before invoking the archive tool.

Eight actual-recipe fixtures, twenty-four transaction and seven audit regressions
pass, including partial archive failure, wrong checksum, manifest corruption,
changed package identity, completed reuse, protected overrides and spaced roots.
`make test-release-local-artifact` uses compiler-independent control routing.
Retained fake-tool success/reuse and failed archive attempts are sealed in
`data/experiments/lifecycle-validation/20261007-local-artifact-transactions`.
No real release ZIP, signer, install or publication ran. Signed artifact/notary
writers, canonical adoption and later independent backup coverage remain open.

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
