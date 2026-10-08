# Shared Shape Stack

Canonical ShapeLib + ShapeAsset sources live here and are forwarded into the three projects.

- Library: `ShapeLib/` (Bezier model, flattening, JSON), `geo/` (ShapeAsset JSON/raster), `tests/` (sanity helper).
- Default asset root: `shared/assets/shapes` (override with `SHAPE_ASSET_DIR`).
- Common env: `SHAPE_ASSET_DIR=/abs/path/to/shared/assets/shapes` before launching any app or converter.

## Converting line_drawing exports
1) Build a converter:
   - `cd ray_tracing && make cli-tools` (preferred), or `cd physics_sim && make shape_asset_tool`.
2) Run the sync helper (defaults to `line_drawing/export -> shared/assets/shapes`):
   ```sh
   SHAPE_ASSET_DIR=shared/assets/shapes shared/shape/sync_exports.sh
   ```
   Pass a custom export dir as the first arg if needed. A manifest is written to `manifest.json` in the output dir.
3) (Optional) Build a sanity tool and rerun the sync; it will auto-run and report bounds/raster success.

Each project reads `SHAPE_ASSET_DIR` if set; otherwise it falls back to its legacy local `config/objects` or `Configs/objects`.


## PhysicsSim Main Edit shape input candidate — 2026-10-07

The PhysicsSim development checkout locally extends its unversioned vendored
non-core shape snapshot with borrowed-text decoding, retaining the existing file
API/decoder semantics. PhysicsSim reuses its own bounded strict JSON reader before
that seam. This candidate is source/test-bound in
`physics_sim/docs/shape_input_admission.md`; canonical shared source, module
versions and ecosystem minimums are unchanged. Reconcile/preserve it during future
managed upstream/vendor adoption; no portfolio rollout or shared release is claimed.


## PhysicsSim Main Edit shape asset publication candidate — 2026-10-07

The local unversioned vendored shape candidate adds owned JSON text serialization
and its allocator-matched release API, checking construction failures/nonfinite
points. PhysicsSim publishes through its existing retained persistence helper;
legacy path-only shared saves still truncate directly. Canonical shared source,
versions, ecosystem minimums and portfolio adoption are unchanged. This local
candidate must be reconciled during managed upstream/vendor adoption. See
physics_sim/docs/shape_asset_publication.md for the host policy and evidence.


## PhysicsSim Main Edit asset text decoder candidate — 2026-10-07

The local unversioned vendored non-core shape candidate adds borrowed text asset
decoding; the existing file API preserves its legacy semantics through the seam.
PhysicsSim picker input reuses its bounded strict JSON reader and app-owned typed
policy before decoding. Canonical shared source, versions and ecosystem minimums
are unchanged. This is a local development candidate requiring managed vendor/
upstream reconciliation, not a portfolio release. See
physics_sim/docs/shape_asset_input_admission.md for tests and remaining startup/
producer-consumer boundaries.
