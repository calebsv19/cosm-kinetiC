# Retained seven-slot scene-cache publication

Main Edit no longer recursively removes previous scene-cache slots before copying.
Under a nonblocking cooperative project lock, it allocates a fresh retained
attempt under physics_sim, records all seven targets and original presence,
and syncs an exclusive pending marker before copying. Four artifact directories
and three manifests are staged and synced before the first target mutation.
The complete predecessor plan is rechecked, each selected predecessor is renamed
into its unique prior-N slot, and new-N is installed. Parent and attempt directory
syncs follow each rename. No predecessor or failed attempt is recursively deleted.
A completed attempt retains all predecessors, its plan and a completion marker.
Only successful completion removes the attempt-owned pending marker.

An incomplete pending marker blocks subsequent publishers. The public status
entrypoint takes a shared nonblocking lock and refuses pending attempts, including
malformed or linked markers. Process death releases the lock without clearing
the durable hold. Original layout/manifest paths remain compatible. Frame
selection rejects negative values and nonpositive stride; retained-index
arithmetic now avoids signed-int overflow. Manifest stream errors are checked;
both copy streams close even when the first close fails.

Sixteen actual native test methods pass: ten path/ID admission regressions and
six transaction methods. Tests exercise retained seven-slot successful replacement
and repeat publication, copy failure with all visible predecessors unchanged,
all fourteen rename failure boundaries with every original still available,
busy ownership and invalid pending holds, native process death during installation,
and failed first publication with all final slots absent. The actual isolated
status Make contract and supported headless scene-project cache-output fixture
pass against real generated cache artifacts. Controlled syscall faults are compiled into the test
object only; production contains no environment fault switch.

## Remaining requirements

Read-only digest-bound planning and explicit retained rollback are now available
in `cache_publication_recovery.md`. Failed attempts remain held until the supported
recovery completes; deleting the pending marker manually is not recovery. Forward
promotion and archive-backed retirement remain incomplete.
Direct consumers of assets/active paths can still see mixed slots while installation
is underway; they must migrate to an immutable generation/read guard before the
whole cache can be claimed atomically visible. Individual renames are atomic;
the seven-target transaction is not one atomic filesystem operation.

Admitted paths and predecessor/lock/hold witnesses constrain cooperative mutation,
but do not provide complete hostile path-swap confinement, source freeze/content
inventory authentication, disk reservation, aggregate copied-byte quotas or I/O
deadlines. Unconfirmed sync failure after all installation and hold removal may
leave complete new visible output. Lock descriptor close is not a durability
commit. Future recovery must bind exact attempt identity, plan, current/staged/
retained bytes and terminal ownership before mutation. Readonly status parsing
and manifest-relative path handling need their own complete admission audit.

Canonical adoption, independent archive coverage for this new packet, installation,
release and CFD physical accuracy are separate and incomplete.
