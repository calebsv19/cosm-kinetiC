# Covering Make ownership for reference runs

Reference ownership now borrows a covering outer Make/build owner only after
admitting its exact checkout/build root, descriptor array, named inode identities,
kernel lock witnesses and global/root descriptor aliases. The original build
lock modes are reaffirmed; the selected outer root remains exclusive. Reference
cleanup closes only newly acquired reference descriptors and does not close or
unlock borrowed build descriptors.

Borrowing a Make root alone would allow competing recipes to share its exclusive
open description. A separate `reference-<root-hash>.lock` intention hierarchy now
spans the selected reference namespace. Ancestors are shared and the selected
DATA.parent is exclusive, preserving exclusion for equal and overlapping recipe
scopes while allowing unrelated siblings. Ordinary reference owners also acquire
that serial hierarchy alongside the existing build/clean hierarchy. Nested
reference borrowing remains kernel-verified. An unrelated outer build profile is
not borrowed; invalid metadata for a covering owner holds rather than silently
falling back.

The reference environment envelope binds repo, data, optional covering build root
and all verified descriptors. Frozen compiler/numerical supervisors inherit the
complete array. Borrowed build mode is preserved on child validation and handoff.
Kernel ownership remains conservative if inherited descendants survive parent
closure; complete descendant termination remains unverified.

Five focused methods prove covering-parent mode/lifetime, same-Make equal and
overlapping scope exclusion, independent siblings, forged aliases/unheld correct
inode rejection, an actual supervised child handoff, and a real parallel Make
execution through the outer build_owner. The parallel proof uses normal Make
recipe descriptor inheritance rather than only manually populated metadata.
Existing reference ownership, record admission, source/root, compiler transaction,
CFD supervisor and build-owner regressions remain separate validation scopes.

No solver/compiler command changed. All 194 wrappers consume this helper through
source identity and frozen supervision. Family numerical probes are synthetic and
native factor libraries controlled; this does not qualify physical accuracy or
real factor ABI. No shared module API/version, canonical adoption, installation,
release, independent archive copy or pruning occurred.

Remaining scope includes hostile path-swap confinement, complete operational-job
ownership, aggregate workflow/storage budgets, coherent interruption recovery,
terminal class eligibility, archive-backed retirement/pruning and canonical
adoption. This closes the documented covering-Make borrowing gap for the tested
cooperative local protocol, not the entire TL03/TL09 lifecycle contract.

Evidence packet: `data/experiments/lifecycle-validation/20261007-reference-make-borrowing`.
