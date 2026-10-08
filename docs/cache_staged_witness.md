# Cache publication staged witnesses

An actual native timing fault changed an earlier staged frame after its complete
source/copy comparison, while a later staged physics manifest was being compared.
The prior publisher accepted both changed finite frames and all three changed
generated manifests (five cases). Thus per-directory readback did not establish
that all seven staged slots still matched their observed state at publication.

The bounded source witness helper now also captures every regular file in all
four staged directories immediately after preparation/sync, before semantic and
byte validation. All three generated manifest files receive named stat witnesses.
The complete source/staged set is rechecked after comparisons and before the first
predecessor displacement. Each remaining staged slot is checked again immediately
before its own predecessor is displaced. Checks retain the existing device/inode,
mode, link-count, size and nanosecond mtime/ctime policy; staged directory witnesses
also detect added/removed entries. Source selection stays bound to existing copy
predicates; staged inventories capture all entries. No shared API/version change.

Each tree enumerates at most 10,000 entries and uses a fixed 10,000-entry witness
allocation. At most five such arrays (source plus four staged directories) live
at once, or 50,000 host-sized name/stat entries. All are freed on success/failure.
This bounds metadata memory independently of raw frame size. Byte/time budgets
remain those documented in cache_copy_integrity.md and cache_source_witness.md.

Native probes cover both earlier frame directories, all three generated manifests,
and a same-byte staged rewrite. A separate rename-boundary probe changes a future
staged frame after whole-plan checks and during first-slot displacement: the later
slot guard holds before displacing its predecessor. Status is held; digest-bound
explicit rollback restores all originals and retains both published and unpromoted
new output. This is recovery evidence for partial publication, not atomic seven-slot
visibility or proof that no observer can see mixed generations.

Witnesses detect ordinary local drift but are not cryptographic source/candidate
authentication. Generated manifest intent still needs complete semantic readback;
these witnesses detect drift after capture, not a writer that initially emitted
wrong bytes. They are in-memory, not a durable authenticated inventory for forward
recovery. Races between each final check and rename remain; directory ancestors
and readers are not pinned into immutable generations. Hard deadlines, workflow
quotas, retirement, canonical adoption and installed/downstream/numerical gates
remain open. Status still lacks authenticated detection of arbitrary finite
active-cache changes. No deletion, solver or binary-format change was introduced.

Evidence: data/experiments/lifecycle-validation/20261007-cache-staged-witness.
