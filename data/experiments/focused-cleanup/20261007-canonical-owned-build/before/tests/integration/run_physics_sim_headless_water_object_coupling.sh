#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="$PHYSICS_DIR/tmp/headless_water_object_coupling"
SUMMARY="$OUT_DIR/run_summary.json"
PROGRESS="$OUT_DIR/run_progress.json"
RUN_DIR="$OUT_DIR/volume_frames/Water Basin"
WATER_MANIFEST="$RUN_DIR/water_manifest_v1.json"
SURFACE="$RUN_DIR/water_surface_000005.json"

rm -rf "$OUT_DIR"
"$PHYSICS_DIR/physics_sim_headless" \
  --water-mode \
  --frames 6 \
  --sim-steps-per-frame 2 \
  --grid 24x16x24 \
  --water-level 0.58 \
  --water-object-fixture \
  --output-root "$OUT_DIR" \
  --summary "$SUMMARY" \
  --progress "$PROGRESS" \
  --overwrite \
  --save-volume-frames

test -f "$SUMMARY"
test -f "$PROGRESS"
test -f "$WATER_MANIFEST"
test -f "$SURFACE"

python3 - "$SUMMARY" "$WATER_MANIFEST" "$SURFACE" <<'PY'
import json
import sys

summary_path, manifest_path, surface_path = sys.argv[1:]

with open(summary_path, "r", encoding="utf-8") as f:
    summary = json.load(f)
with open(manifest_path, "r", encoding="utf-8") as f:
    manifest = json.load(f)
with open(surface_path, "r", encoding="utf-8") as f:
    surface = json.load(f)

def require(condition, message):
    if not condition:
        raise SystemExit(message)

require(summary.get("status") == "passed", "headless water object run failed")
require(summary.get("mode") == "water", "run did not use water mode")
require(summary.get("water_object_fixture") is True, "summary did not enable object fixture")

manifest_object = manifest.get("object_coupling") or {}
require(manifest_object.get("enabled") is True, "manifest object coupling disabled")
require(
    manifest_object.get("fixture_id") == "water_pool_submerged_solid",
    "manifest fixture id mismatch",
)
require(
    manifest_object.get("response_mode") == "solid_mask_plus_smoothed_export_sidecar_displacement_deterministic_wake",
    "manifest response mode mismatch",
)

surface_summary = surface.get("summary") or {}
object_coupling = surface_summary.get("object_coupling") or {}
require(object_coupling.get("enabled") is True, "surface object coupling disabled")
require(object_coupling.get("fixture_active") is True, "surface fixture inactive")
require(int(object_coupling.get("object_solid_cells", 0)) > 0, "no object solid cells")
require(int(object_coupling.get("object_footprint_columns", 0)) > 0, "no object footprint columns")
require(int(object_coupling.get("object_wet_overlap_cells", 0)) > 0, "no wet/object overlap")
require(float(object_coupling.get("displaced_volume_m3", 0.0)) > 0.0, "no displaced volume")
require(object_coupling.get("displacement_applied") is True, "no displacement applied")
require(
    float(object_coupling.get("displacement_delta_max_m", 0.0)) >
    float(object_coupling.get("displacement_delta_min_m", 0.0)),
    "displacement delta range is flat",
)
require(int(object_coupling.get("displacement_sample_count", 0)) > 0, "no displacement samples reported")
require(float(object_coupling.get("displacement_weight_sum", 0.0)) > 0.0, "no displacement weight sum")
require(float(object_coupling.get("displacement_weight_max", 0.0)) > 0.0, "no displacement weight max")
require(float(object_coupling.get("displacement_delta_abs_sum_m", 0.0)) > 0.0, "no displacement delta energy")
require(float(object_coupling.get("displacement_delta_rms_m", 0.0)) > 0.0, "no displacement delta rms")
require("displacement_capped_sample_count" in object_coupling, "missing capped sample count")
require(float(object_coupling.get("object_zone_height_stddev_m", -1.0)) >= 0.0, "missing object zone height stddev")
require(float(object_coupling.get("object_zone_max_slope", 0.0)) > 0.0, "missing object zone slope")
require(float(surface_summary.get("max_slope", 0.0)) > 0.01, "water surface stayed too flat")
require(surface_summary.get("finite_normals") is True, "surface normals are not finite")
require(len(surface.get("heights_y") or []) == int(surface.get("sample_count", 0)), "height count mismatch")

print(json.dumps({
    "fixture": object_coupling.get("fixture_id"),
    "object_solid_cells": object_coupling.get("object_solid_cells"),
    "object_wet_overlap_cells": object_coupling.get("object_wet_overlap_cells"),
    "displaced_volume_m3": object_coupling.get("displaced_volume_m3"),
    "displacement_delta_min_m": object_coupling.get("displacement_delta_min_m"),
    "displacement_delta_max_m": object_coupling.get("displacement_delta_max_m"),
    "displacement_sample_count": object_coupling.get("displacement_sample_count"),
    "displacement_capped_sample_count": object_coupling.get("displacement_capped_sample_count"),
    "displacement_delta_rms_m": object_coupling.get("displacement_delta_rms_m"),
    "object_zone_height_stddev_m": object_coupling.get("object_zone_height_stddev_m"),
    "object_zone_max_slope": object_coupling.get("object_zone_max_slope"),
}, indent=2))
PY

echo "physics_sim headless water object coupling passed: $SURFACE"
