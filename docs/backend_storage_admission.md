# 2D backend initial storage admission

PhysicsSim Main Edit now admits the complete initial 2D backend allocation set
before its first allocation and publishes a backend only when every required
field exists. Oversize/invalid grids return NULL before allocation. Failure of
any auxiliary mask/velocity/distance allocation destroys the entire candidate
through the existing backend destructor. Fluid construction failure now uses the
same candidate destructor. No partially usable backend is returned.

The app-owned read-only backend_2d_initial_storage_bytes helper bounds requested
initial storage to 256 MiB: sizeof backend/state/fluid plus seven fluid float
fields, two byte masks and three auxiliary float fields per cell. Division before
multiplication prevents overflow and oversized requests. It requires both grid
axes >=2 and preserves its output on refusal. Accounting is paired with native
allocation instrumentation; actual accepted allocations must sum to the planned
byte count. This is requested storage, excluding allocator metadata, libraries,
process RSS, later emitter masks, raster candidates, GUI or export buffers. It is
not a hard RSS/workflow quota. Oversized grids which previously attempted larger
allocation are now refused by this host policy. Configuration and native pointers
remain borrowed trusted input during construction; live configuration mutation
and later state/grid consistency still require qualification.

Reuse: existing Fluid2D construction/destruction, backend destructor, atmosphere
seeding, runtime dispatch and fields remain in use. This is app resource/ownership
policy, not a new core_math/core_space semantic or solver algorithm. No shared
source/API/version or minimum-adoption change. Standalone fluid2d_create is not
changed or claimed globally budgeted. Its direct callers remain an audit surface.

Validation passes 48 distinct methods: 26 construction methods and 22 freshly
rebuilt real-runtime import-mask regressions. The 26 construction methods also
pass targeted ASan/UBSan/float-cast-overflow instrumentation; repeated sanitizer
methods are not added to the distinct count. The actual backend and Fluid2D
allocation/free calls are instrumented with source-only test wrappers. All 15
reached allocation boundaries are faulted separately; each construction returns
NULL with no tracked live allocation or foreign/double free. Ordinary construction
makes 15 allocations whose requested bytes match the planner exactly. Minimum,
rectangular, normal and seeded construction retain normal field/view behavior.
Zero/one/negative/large/INT_MAX grids and NULL config refuse before allocation;
exact budget-near planning and one-row-excess refusal are read-only and do not
allocate a real 256 MiB test object. Linked support objects are not claimed fully
sanitizer-instrumented; fault injection is controlled allocation refusal, not a
real OS memory exhaustion exercise.

Both existing Make backend/runtime-field contracts pass in a separate isolated
profile. The actual production backend object compiles via Make. Four safe old-
constructor ordinary controls pass, and all five auxiliary allocation faults
incorrectly publish an old backend; those nine cases are terminal and bounded.
No crashing/hanging historical sanitizer test was launched this slice. The prior
import-mask old-code child PID68238 remains UE with unreaped parent PID68221 in
the fresh retained observation (session28919). It was not restarted or overwritten;
older unresolved sanitizer holds also remain. Current construction/regression/
contract/build/proof sessions are terminal, not proof of prior-child reaping.

The fresh backend-storage-admission-20261007 profile preserves normal PGM/asset
bytes against retained baseline and preserves source hashes. Matching repeat
preserves 18 outputs. Read-only clean preview admits 34 owned files; no cleanup
applies. The separate contract profile retains its own outputs. Pre-existing
backend/CFD changes are preserved; a pre-turn snapshot and incremental patch are
retained. No complete GUI link, installed/runtime human acceptance, release or
numerical CFD accuracy qualification is claimed.

Next: audit runtime configuration/state dimensions before accesses, initial/later
field ownership and emitter/scratch allocation lifetime. Complete aggregate runtime
bounds, full process recovery, retirement/pruning, canonical adoption and newer
independent backup coverage remain open. This packet is outside the frozen seventy-
packet backup. No commit, canonical/shared mutation, installation, release, remote
transfer or user-evidence deletion occurred. Full TL01-TL13 remains active.

Evidence: data/experiments/lifecycle-validation/20261007-backend-storage-admission.
