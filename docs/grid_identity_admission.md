# 2D runtime allocation grid identity

SimRuntimeBackend2D now records allocation_w/allocation_h at construction.
Construction snapshots the selected configuration dimensions once and uses those
same values for admission, Fluid2D creation, metadata and auxiliary allocation.
The app-owned private header supplies storage validity and config/scene match
checks. Allocation extents are immutable by owner contract; initialized truthful
native pointers and arrays remain required. This is not authentication against
arbitrary C memory corruption or concurrent malicious owner mutation.

Every identified scene/config-dependent 2D mutation entrypoint now checks exact
axis equality against allocation metadata before access, allocation, free or flag
changes: static/import obstacle construction, combined/dynamic obstacles, distance
work, emitter-mask replacement/application, boundary application/enforcement,
obstacle enforcement, object-motion injection, brush and step. Step admits both
scene configuration and its separately supplied configuration. Same-cell-count
axis swaps also refuse. Mismatches leave current fields, flags and old emitter
masks intact; restoring the original configuration permits the operation again.
A changed grid requires a separately created backend, not in-place reinterpretation.

Fluid dimension disagreement with allocation metadata also refuses local clear,
uniform seeding, valid checks, snapshot publication, views, report and compatibility
activity before output mutation. Views still describe the allocated grid when
only an unrelated editable config differs. Standalone field contract fixtures now
explicitly declare their actual allocated dimensions, including an intentionally
fluid-free obstacle fixture; this does not add fluid allocation to that fixture.
Destruction remains available regardless of admission failure so held candidates
can release their owned storage.

Reuse is app-owned policy composed with existing backend/Fluid2D lifetimes and
runtime routes. core_math/core_space do not own this storage extent identity.
No shared module source/API/version or adoption minimum changes. There is no new
solver or numerical-method change, automatic resize, rebuild or GUI installation.
The checks do not yet admit all numeric object/emitter payloads or bounded native
inventories, and do not replace global emitter/scratch budgets or transaction
qualification. Stored extents and pointers must remain truthful throughout life.

Validation passes 67 distinct methods: 18 new identity methods, 27 freshly rebuilt
construction methods and 22 freshly rebuilt import-mask methods. The identity 18
and construction 27 also pass targeted ASan/UBSan/float-cast-overflow runs; repeated
methods are not added to the distinct count. Real full backend/runtime-field
translation units are linked with actual fluid, lookup, builder and shape input
support. Source-only allocation/free hooks prove twelve mutation routes refuse
before any allocation/free across grow, shrink, same-count swap, zero, negative,
INT_MAX, fluid disagreement and absent metadata cases. Test snapshots cover state,
fluid fields, masks, dirty flags and old emitter contents. Local/read-only refusals
preserve outputs and prior snapshot files; matching operation and restored-config
controls work. A constructor allocation hook changes borrowed config to INT_MAX
at first allocation: the constructed grid and exact byte accounting retain the
original admitted dimensions. Invalid huge requests in the test allocator are
refused before actual OS allocation. Support objects are not all instrumented.

Both existing Make 2D backend/runtime-field contracts pass with the updated extent
contract. Twelve bounded old-code shrink cases are terminal: eleven violate the
new nonmutation contract; the enforce-boundary control happens to leave this
fixture unchanged. No unsafe growth or crashing historical sanitizer control was
launched. The earlier import-mask countertest child PID68238 remains observed UE
with unreaped parent PID68221 (session28919); it was neither restarted nor claimed
reaped. Current tests/builds/proofs are terminal and separate.

The fresh grid-identity-admission-20261007 profile compiles both actual changed
runtime objects and shape tools. Normal tool bytes match retained baseline and
source hashes stay unchanged. Matching repeat preserves 19 outputs; read-only
clean preview admits 36 owned files. No cleanup applies. Pre-turn source snapshots
and incremental diffs preserve unrelated CFD/backend work. No complete GUI link,
installed product, human acceptance or numerical CFD qualification is implied.

Remaining work includes native numeric/inventory admission, later emitter/scratch
allocation and publication lifetime, complete process recovery and resource limits,
evidence retirement, canonical adoption and independent newer backup coverage.
This packet is outside the frozen seventy-packet snapshot. No commit, canonical/
shared mutation, install, release, remote transfer or user-evidence deletion
occurred; full TL01-TL13 remains active.

Evidence: data/experiments/lifecycle-validation/20261007-grid-identity-admission.
