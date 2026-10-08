# 2D runtime import mask admission and publication

backend_2d_rasterize_import_to_mask now requires mask_count to equal the configured
grid cell count, admits positive multi-cell dimensions and a 64 MiB per-mask bound
using division before multiplication, and rejects invalid/unterminated paths,
nonfinite import transforms and inconsistent library storage/count before touching
caller storage. Selected cached assets pass the existing typed host admission
before shared bounds access. The initialized library and truthful allocated caller
buffer remain trusted native contracts; this is not memory-pointer authentication.

Failure leaves caller bytes unchanged. The asset path already builds an isolated
PhysicsObject; it copies only on success. The raw-shape fallback now rasterizes
into a bounded candidate and copies only after success, then releases candidate
storage. A successfully loaded empty document is freed before refusal. The helper
no longer clears the caller at entry or publishes partial failed raster output.
Existing internal callers pass exactly their grid cell count. Native API comments
state the exact-count and nonmutation contract. Ordinary accepted mask bytes are
unchanged. Peak memory can include caller, candidate, input and other runtime
storage; this per-mask cap is not an aggregate runtime allocation quota.

Reuse: the real existing lookup, admitted shape reader, typed ShapeAsset admission,
PhysicsObject builder and both rasterizers are retained. Candidate publication
is app-owned in-memory runtime policy. core_math/core_space do not supply this
storage lifetime contract. No shared source/API/version or adoption metadata
changes. The existing no-asset backend contract adds an explicit admission stub
because its lookup is always NULL; real asset behavior is separately linked and
exercised by the new native integration harness.

Validation passes 49 distinct methods: 22 import-mask methods plus 27 unchanged
builder regressions. The 22 import-mask methods also pass targeted ASan/UBSan/
float-cast-overflow instrumentation; that repeat is not added to the distinct
count. The harness compiles and links the actual full backend translation unit,
backend fields, fluid implementation, lookup/project code, builder, real asset
reader and shape reader/raster support. Fault-only backend macros control calloc,
empty loaded documents and a partial failed raw raster, while normal paths call
the real implementations. Already-built JSON/ShapeLib and other support objects
are not claimed fully sanitizer-instrumented. Cases cover short/long/zero/SIZE_MAX
counts, zero/one/negative/large/INT grids, paths, invalid libraries, nonfinite
transforms, missing/malformed input, empty documents, allocation and raster failure.
Accepted cached and raw masks match their existing raster routes, including
untouched bytes after the exact mask boundary.

Both existing actual Make contracts pass in a separate fresh profile:
test-sim-runtime-backend-2d-contract and
test-sim-runtime-backend-2d-runtime-fields-contract. They retain their ordinary
boundary-flow, motion and runtime-field assertions. The affected production
backend object compiles through the regular isolated Make graph.

Eleven bounded safe old-code cases are terminal: two ordinary parity controls pass
and nine fail the new refusal/nonmutation contract. Four of the nine are nonfinite
transform subcases, not separately counted methods. An additional old-code
sanitized oversized-copy countertest did NOT finish: exec session 28919 owns
parent PID 68221 and child PID 68238. After its timeout/termination path the child
was observed in UE state and the parent remained an unreaped waiter. No terminal
sanitizer diagnostic or successful reaping is claimed. Its binary, session and
parent are preserved; do not restart or overwrite them. The current-code gates,
builds and proofs are terminal and separate. Older unresolved sanitizer holds
also remain outside this proof. The packet seals an observation of this unresolved
countertest, not a completed lifecycle or all-processes-terminal claim.

Fresh import-mask-admission-20261007 tool/production-object profile retains normal
PGM/asset byte parity and source hashes. Matching repeat preserves 18 outputs;
read-only clean preview admits 34 owned files. No cleanup applies. Existing
pre-incident/CFD changes in this backend were preserved; the packet includes the
pre-turn source and a bounded incremental diff. No GUI link, installed-product,
public release or human acceptance is implied.

Next related gap: sim_runtime_backend_2d_create allocates the fluid and several
auxiliary fields without an explicit aggregate host budget, and does not reject
partial auxiliary allocation failure. Runtime-field allocation/config lifetime,
collider/render consumers, hard global bounds, process recovery, evidence
retirement, canonical adoption and independent newer archive coverage remain open.
This evidence is outside the frozen seventy-packet snapshot. No commit, canonical
or shared mutation, install, release, transfer or deletion occurred. The full
TL01-TL13 goal remains active.

Evidence: data/experiments/lifecycle-validation/20261007-import-mask-admission.
