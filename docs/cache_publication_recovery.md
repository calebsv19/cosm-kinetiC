# Scene-cache recovery planning and retained rollback

The supported local recovery entrypoint is read-only by default:

```sh
python3 -B scripts/cache_publication_recovery.py --project /absolute/project
```

It acquires a shared nonblocking existing cache owner lock, binds the exact pending
attempt, admits the seven fixed layout destinations and rejects unknown journal
fields/duplicate keys and unknown attempt entries. Nofollow bounded regular reads
and streaming SHA-256 inventories refuse links, special or hardlinked files.
Observation bounds are 10,000 entries, depth 16, 8 GiB per file and 32 GiB combined
observed bytes, with a sampled 120-second wall bound. Whole-plan entry identities,
including recorded absence, are rechecked after all slot observations.

The result contains a `plan_sha256` over the current journal, pending identity,
slot content digests and file/directory witnesses. It does not promote staged
output and never clears a hold in read-only mode. Recovery refuses ambiguous or
missing original states. The native publisher now records absolute target and
attempt paths so future inspection is independent of the original working directory.
Older relative journals require their original working directory and are held if
that interpretation does not match the selected project.

An explicit retained rollback uses the exact current plan digest:

```sh
python3 -B scripts/cache_publication_recovery.py --project /absolute/project \
  --rollback --expected-plan-sha256 DIGEST_FROM_READ_ONLY_PLAN
```

Under the same exclusive nonblocking native lock, it reinspects the complete
plan, rejects drift, rechecks each selected slot and owner namespace, moves new
visible output into unique `rollback-new-N` slots and restores `prior-N`
predecessors. Originally absent destinations become absent again while displaced
new bytes remain retained. It syncs affected directories, verifies the content of
all restored destinations, writes a retained `rolled-back` marker and only then
releases the matching pending hold. No cache tree, staged candidate, predecessor
or attempt is recursively deleted. Interrupted rollback retains the hold and can
be replanned from its new digest; no stale plan is reused.

Twenty-five native/recovery methods pass: sixteen inherited native transaction
methods plus nine recovery methods. Actual native process death is injected at
all fourteen publication rename boundaries, then each fixture is planned and
rolled back with exact original content verified. Additional cases cover wrong
or changed digests, duplicate/out-of-scope journals, ownership/link/hardlink/unknown
entry holds, interrupted rollback resumption, partial-copy rollback, relative
publication inspected from another cwd and whole-pass drift after an earlier
slot observation. The rebuilt actual status Make contract and supported headless scene-project
cache-output fixture also pass. The operator CLI defaults to read-only, refuses
rollback without a digest and verifies matching-digest rollback.
These use disposable controlled artifacts; no user cache was
recovered or retired during qualification.

## Limits and remaining work

This rollback resolves a held cooperative local cache; it does not authenticate
historical provenance beyond the admitted publisher journal and current retained
bytes. The publication journal lacks a sealed complete candidate content inventory,
so forward promotion is deliberately unavailable. Immutable-generation readers,
full hostile path-swap confinement, aggregate storage quotas, hard I/O deadlines,
archive-backed attempt retirement and installed/downstream qualification remain
open. The sampled deadline cannot interrupt a blocked filesystem read or hash.
An unconfirmed sync failure after pending removal may leave a complete restored
layout visible. Repeated rollback after successful hold release is not an automatic
mutation retry: inspect retained terminal evidence. Canonical adoption and backup
coverage for this new work remain incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-cache-publication-recovery.
