# PhysicsObject asset raster admission

The PhysicsSim runtime asset-to-mask builder now admits the whole draw before
allocating mask storage or modifying its caller. Its output must be initialized
with mask == NULL; nonempty output is refused without losing owned storage.
Every failure leaves all caller fields unchanged, including allocation failure.
Successful output still uses the existing shared shape_asset_rasterize routine.

App policy bounds each mask to 64 MiB using division before multiplication,
reuses physics_sim_shape_asset_admitted for schema/structural/finite input bounds
(1,024 paths and 10,000 total points), rejects nonfinite native base/options,
and bounds local coordinates to sqrt(FLT_MAX)/32. The guard mirrors the shared
routine's float transform order, validates mapped endpoints before integer
rounding, leaves INT_MAX/8 headroom and charges all segment square visits plus
closed-polygon grid/edge tests against 64*1024*1024 work units. Off-grid segments
still count. All selected paths share the one budget; individual small paths
cannot evade aggregate refusal. Inverse fill corner coordinates are also bounded.
These bounds are per call, not global workflow/heap/storage quotas or a wall-clock
deadline. Caller-provided native pointers and truthful allocated arrays remain a
trusted-local contract. The output object must be initialized before use.

Reuse decision: reuse-adopted for the existing shared ShapeAsset rasterizer and
host typed asset admission. Existing shape_import_rasterize supplies the prior
app work-policy pattern; ShapeAsset cannot directly use its Shape/Bezier input.
core_math provides general float vectors and core_space owns placement/frame
mapping; neither owns this raster algorithm or host allocation/work policy.
The admission adapter therefore stays in the existing app-owned PhysicsObject
builder. No shared source/API/version or adoption minimum changes. Arithmetic
mirroring is a compatibility dependency and must be requalified if the shared
raster algorithm changes. Standalone shared raster APIs remain legacy/unbounded.

Validation passes 61 distinct methods: 27 native builder methods and 34 current
startup-library regressions. The 27 builder methods also pass when the actual
builder and shared ShapeAsset raster implementation are compiled with Address-
Sanitizer, UndefinedBehaviorSanitizer and float-cast-overflow instrumentation;
linked support libraries are not claimed fully instrumented. Fixtures cover
count/pointer/schema/source admission, nonfinite base/options, integer/zero/large
grids, extreme rotation/scale/stroke, off-grid and aggregate segment work, polygon
fill work, nonempty output and controlled allocation failure. A source-only calloc
wrapper proves refused workloads allocate no builder mask. An exact 64 MiB grid
reaches controlled allocation refusal; this proves admission, not successful real
allocation at that size. Normal open/closed/fitted/default cases have byte parity
with the unchanged shared rasterizer. All eight checked-in asset masks have that
parity, and their input hashes remain unchanged.

Fifteen safe old-builder control cases were run without extreme legacy raster
loops: four normal parity cases pass, while eleven countercases fail the new
contract (six are nonfinite-option subcases). This is not eleven distinct test
methods and does not claim historical extreme loops completed. All new sessions
are terminal; no new held worker was created. Older unresolved sanitizer workers
were neither restarted nor claimed reaped.

The isolated object-raster-admission-20261007 profile compiles the actual builder
object and both shape tools. Normal PGM/asset bytes match retained baseline.
The initial repeat changed compiler resolution through PATH from Apple Clang to
Homebrew Clang; recorded configuration identity correctly changed and rebuilt.
That failed no-op assertion and its log are retained. A final repeat under the
same compiler identity preserves 18 object/binary/manifest outputs. Clean preview
admits 35 owned files; no cleanup was applied. No final GUI link, installed product
or human runtime acceptance is claimed.

A further caller gap was found: backend_2d_rasterize_import_to_mask currently
trusts mask_count without matching it to configured grid dimensions before clear,
raster and copy. It must be admitted before claiming the complete runtime import
pipeline safe. Earlier backend allocation/setup, collider/render consumers,
coherent asset identity, global bounds, recovery/retirement, canonical adoption
and independent archive coverage also remain open. This packet is outside the
frozen seventy-packet backup. No commit, canonical/shared mutation, install,
release, transfer or evidence deletion occurred; full TL01-TL13 remains active.

Evidence: data/experiments/lifecycle-validation/20261007-object-raster-admission.
