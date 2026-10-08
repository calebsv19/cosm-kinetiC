# Shape asset producer/consumer policy

PhysicsSim Main Edit asset publication now shares the picker reader's app-owned
contract. Common header constants declare 16 MiB output, 1,024 paths, 10,000 total
points and a 1 MiB decoded-name upper bound. The existing strict JSON scanner also
bounds each encoded string token to 1 MiB including quotes/escapes; this can impose
a smaller decoded-name limit. The reader's original encoded-token policy is
unchanged. Native schema <= 0 retains the shared serializer's schema-1 default;
unsupported positive schemas refuse before publication.

Before constructing serializer trees, physics_sim_shape_asset_admitted rejects
excess path/aggregate-point counts, missing arrays for positive counts, excessive
names and nonfinite native points. The publisher then calls the existing shared
serializer and physics_sim_shape_asset_text_admitted. The latter reuses the exact
strict JSON parser and typed asset policy used by file input. Only admitted bytes
enter retained persistence. Redundant separate name-only JSON construction was
removed. No wire serializer, parser, persistence engine or shared module API was
duplicated/changed in this slice; this extends app-owned policy reuse.

This closes the known large/schema-invalid generated-asset mismatch: an asset
that exceeds reader structural/string/schema limits refuses before a destination
stage/replacement rather than publishing unreadable bytes. These are acceptance
checks, not a hard heap quota or proof that future file observation succeeds under
I/O errors, concurrent drift or unconfirmed post-rename durability. Strict reader
path policy and ordinary runtime availability still apply independently.

Native countertests use the preceding retained profile's publisher and host
reader. Across five original methods, three failures reproduce successful
replacement followed by reader refusal for 1,025 paths, 10,001 points and schema 2.
The normal and exact 10,000-point cases already round-trip. Real source assets are
untouched; predecessors in disposable roots demonstrate the previous replacement.

Final validation passes 99 distinct methods: 17 actual producer/reader round trips,
17 strict asset file input, 26 picker/drop, 17 editor conversion publication,
15 CLI publication and seven actual Make/link methods. Round-trip cases cover
normal/default schema, exact/over path and aggregate-point bounds, missing arrays,
nonfinite native points, oversized names, exact/over encoded escaped-name tokens,
exact plain-name token and maximum finite float. Refused publication preserves the
predecessor with no candidate stage. Accepted boundary outputs are reloaded through
the real host file reader. Existing picker, conversion and publication tests pass.

An initial twelve-method gate had one failed expectation: it treated the 1 MiB
string limit as decoded bytes and expected a much larger encoded escaped token
to pass. Inspection established the existing encoded-token limit. The failed log
is retained and excluded from final counts; corrected positive/negative tests
qualify the existing reader behavior without loosening it.

Fresh asset-roundtrip-policy-20261007 builds both tools and three affected editor
translation units. Normal PGM/asset bytes match unchanged retained baseline;
source remains unchanged. Matching repeat preserves 20 object/binary/manifest
outputs. Read-only clean preview admits 38 owned files; no cleanup applies. All
current build/test/proof sessions are terminal. Older held sanitizer workers are
not restarted or claimed reaped. Canonical PhysicsSim remains clean/unchanged.

Startup inspection confirms main still calls legacy shared shape_library_load_dir.
That loop reads through the legacy file API, skips failed loads/allocation growth
and returns a partial library in readdir order. The next slice should use the
admitted host loader with bounded directory/aggregate work, deterministic identity
and truthful partial/failure semantics. This slice does not claim startup input,
complete directory snapshots, geometry/raster safety, GUI acceptance or shared
consumer rollout. Recovery, quotas, retirement, canonical adoption and independent
newer archive coverage remain open. This packet lies outside the frozen seventy-
packet snapshot. No commit, canonical mutation, version change, install, release,
remote transfer or user-evidence deletion occurred. Full TL01-TL13 remains incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-asset-roundtrip-policy.
