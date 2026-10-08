# Cooperative reference namespace ownership

All 194 reference wrappers now acquire ownership around their entire public run:
preparation, source freezing, factor/tool commands, numerical execution, artifact
hashing, receipt publication and cached readback. Invalid names and inadmissible
selected paths remain held. The selected exclusive scope is DATA.parent because
run families also share its supervisor-source directory. Sibling families within
that parent serialize; unrelated selected parents remain independent.

The descriptor hierarchy follows existing build_owner paths: shared clean.lock,
shared ancestor build-<root-hash>.lock intentions and exclusive selected-root lock.
Legacy build reference paths therefore cooperate with the current guarded Main
Edit clean/build controls. Evidence-root families use the same lock naming and
ancestor protocol below data/experiments. Canonical's unguarded clean is not
protected by a separate checkout's locks. A covering outer build scope can now be borrowed after kernel verification,
with a separate reference serial hierarchy for competing Make recipes. See
`reference_make_owner_borrowing.md`. Unrelated profiles retain independent admission.

Lock paths/components must be admitted; opened descriptors must be regular,
single-linked and match their named inode. Nonblocking kernel lock acquisition
holds competing writers and cleanup. Environment metadata conveys descriptors;
it is never accepted as ownership by itself. Each inherited descriptor is compared
with its named lock and tested against a separate lock witness; ownership is
reaffirmed through the claimed descriptor. Nested calls can borrow an already
verified covering reference scope. A main-thread guard prevents competing threads
from racing mutation of process-wide inherited ownership metadata.

The frozen supervisor passes verified ownership descriptors through its live
anchor to compiler/tool and numerical commands. Closing the parent's descriptors
does not explicitly unlock a shared open description; an inherited child can keep
the scope held. This is conservative exclusion, not proof that all descendants
terminated. Existing receipts continue to mark complete descendant termination
unverified. Lock files are inert namespace entries, never reset or deleted here.

Reuse decision: the existing build/clean kernel convention is adopted and compared
against build_owner. A self-contained frozen Python adapter is retained because
minimal solver fixtures/capsules do not ship checkout control modules, and the
shared C core_jobs/core_workers in-process APIs do not own this Python descriptor
handoff. No shared module behavior/version changed.

Behavior proof covers same-parent conflict, independent siblings, cleanup/build
hierarchy exclusion, linked/special lock holds, nested borrowing, forged descriptors,
an inherited child retaining exclusion after parent closure, and an actual
supervised command verifying its inherited locks. Root/source and full reference
family regressions remain separate evidence. Numerical commands are synthetic
control probes; real controlled factor libraries do not qualify CFD ABI/accuracy.

This does not establish an adversarial path-swap sandbox, authenticated worker
identity, complete operational-job ownership, global resource quotas, interruption
recovery, terminal retirement eligibility, archive-backed pruning or canonical
adoption. Those requirements remain open.

Evidence packet: `data/experiments/lifecycle-validation/20261007-reference-namespace-ownership`.
