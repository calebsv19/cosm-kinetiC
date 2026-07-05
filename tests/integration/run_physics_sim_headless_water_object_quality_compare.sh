#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="$PHYSICS_DIR/tmp/headless_water_object_quality_compare"
SUMMARY_JSON="$OUT_DIR/wtr65_quality_compare_summary.json"
SUMMARY_TXT="$OUT_DIR/wtr65_quality_compare_summary.txt"

BASELINE_GRID="${WTR65_BASELINE_GRID:-24x16x24}"
BASELINE_FRAMES="${WTR65_BASELINE_FRAMES:-6}"
BASELINE_STEPS="${WTR65_BASELINE_SIM_STEPS_PER_FRAME:-2}"
QUALITY_GRID="${WTR65_QUALITY_GRID:-36x18x36}"
QUALITY_FRAMES="${WTR65_QUALITY_FRAMES:-8}"
QUALITY_STEPS="${WTR65_QUALITY_SIM_STEPS_PER_FRAME:-3}"
WATER_LEVEL="${WTR65_WATER_LEVEL:-0.58}"
MAX_OBJECT_ZONE_STDDEV_M="${WTR65_MAX_OBJECT_ZONE_STDDEV_M:-0.010}"
MAX_OBJECT_ZONE_SLOPE="${WTR65_MAX_OBJECT_ZONE_SLOPE:-0.050}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

run_profile() {
  local label="$1"
  local grid="$2"
  local frames="$3"
  local steps="$4"
  local profile_root="$OUT_DIR/$label"

  "$PHYSICS_DIR/physics_sim_headless" \
    --water-mode \
    --frames "$frames" \
    --sim-steps-per-frame "$steps" \
    --grid "$grid" \
    --water-level "$WATER_LEVEL" \
    --water-object-fixture \
    --output-root "$profile_root" \
    --summary "$profile_root/run_summary.json" \
    --progress "$profile_root/run_progress.json" \
    --overwrite \
    --save-volume-frames
}

run_profile baseline "$BASELINE_GRID" "$BASELINE_FRAMES" "$BASELINE_STEPS"
run_profile quality "$QUALITY_GRID" "$QUALITY_FRAMES" "$QUALITY_STEPS"

python3 - "$OUT_DIR" "$SUMMARY_JSON" "$SUMMARY_TXT" \
  "$BASELINE_GRID" "$BASELINE_FRAMES" "$BASELINE_STEPS" \
  "$QUALITY_GRID" "$QUALITY_FRAMES" "$QUALITY_STEPS" "$WATER_LEVEL" \
  "$MAX_OBJECT_ZONE_STDDEV_M" "$MAX_OBJECT_ZONE_SLOPE" <<'PY'
import json
import math
import os
import sys

(
    out_dir,
    summary_json_path,
    summary_txt_path,
    baseline_grid,
    baseline_frames_text,
    baseline_steps_text,
    quality_grid,
    quality_frames_text,
    quality_steps_text,
    water_level_text,
    max_object_zone_stddev_text,
    max_object_zone_slope_text,
) = sys.argv[1:]
max_object_zone_stddev_m = float(max_object_zone_stddev_text)
max_object_zone_slope = float(max_object_zone_slope_text)

def require(condition, message):
    if not condition:
        raise SystemExit(message)

def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def metric_float(obj, key):
    value = float(obj.get(key, 0.0))
    require(math.isfinite(value), f"{key} is not finite")
    return value

def metric_int(obj, key):
    value = int(obj.get(key, 0))
    require(value >= 0, f"{key} is negative")
    return value

def read_profile(label, grid, frames_text, steps_text):
    frames = int(frames_text)
    steps = int(steps_text)
    run_root = os.path.join(out_dir, label)
    surface_dir = os.path.join(run_root, "volume_frames", "Water Basin")
    summary = load_json(os.path.join(run_root, "run_summary.json"))
    manifest = load_json(os.path.join(surface_dir, "water_manifest_v1.json"))
    surface_path = os.path.join(surface_dir, f"water_surface_{frames - 1:06d}.json")
    surface = load_json(surface_path)
    surface_summary = surface.get("summary") or {}
    obj = surface_summary.get("object_coupling") or {}

    require(summary.get("status") == "passed", f"{label} run did not pass")
    require(summary.get("mode") == "water", f"{label} run did not use water mode")
    require(summary.get("water_object_fixture") is True, f"{label} fixture disabled")
    require((manifest.get("object_coupling") or {}).get("enabled") is True,
            f"{label} manifest object coupling disabled")
    require(obj.get("enabled") is True, f"{label} sidecar object coupling disabled")
    require(obj.get("fixture_active") is True, f"{label} fixture inactive")
    require(surface_summary.get("finite_normals") is True, f"{label} normals not finite")

    result = {
        "label": label,
        "grid": grid,
        "frames": frames,
        "sim_steps_per_frame": steps,
        "water_level": float(water_level_text),
        "surface_path": surface_path,
        "sample_count": metric_int(surface, "sample_count"),
        "grid_w": metric_int(surface, "grid_w"),
        "grid_d": metric_int(surface, "grid_d"),
        "sample_spacing_x": metric_float(surface, "sample_spacing_x"),
        "sample_spacing_z": metric_float(surface, "sample_spacing_z"),
        "surface_min_y": metric_float(surface_summary, "surface_min_y"),
        "surface_max_y": metric_float(surface_summary, "surface_max_y"),
        "surface_avg_y": metric_float(surface_summary, "surface_avg_y"),
        "global_max_slope": metric_float(surface_summary, "max_slope"),
        "object_solid_cells": metric_int(obj, "object_solid_cells"),
        "object_footprint_columns": metric_int(obj, "object_footprint_columns"),
        "object_wet_overlap_cells": metric_int(obj, "object_wet_overlap_cells"),
        "displaced_volume_m3": metric_float(obj, "displaced_volume_m3"),
        "displacement_sample_count": metric_int(obj, "displacement_sample_count"),
        "displacement_capped_sample_count": metric_int(obj, "displacement_capped_sample_count"),
        "displacement_delta_min_m": metric_float(obj, "displacement_delta_min_m"),
        "displacement_delta_max_m": metric_float(obj, "displacement_delta_max_m"),
        "displacement_delta_abs_sum_m": metric_float(obj, "displacement_delta_abs_sum_m"),
        "displacement_delta_rms_m": metric_float(obj, "displacement_delta_rms_m"),
        "displacement_weight_sum": metric_float(obj, "displacement_weight_sum"),
        "displacement_weight_max": metric_float(obj, "displacement_weight_max"),
        "object_zone_height_min_y": metric_float(obj, "object_zone_height_min_y"),
        "object_zone_height_max_y": metric_float(obj, "object_zone_height_max_y"),
        "object_zone_height_avg_y": metric_float(obj, "object_zone_height_avg_y"),
        "object_zone_height_stddev_m": metric_float(obj, "object_zone_height_stddev_m"),
        "object_zone_max_slope": metric_float(obj, "object_zone_max_slope"),
    }
    require(result["sample_count"] > 0, f"{label} has no surface samples")
    require(result["object_solid_cells"] > 0, f"{label} has no object cells")
    require(result["object_wet_overlap_cells"] > 0, f"{label} has no wet object overlap")
    require(result["displacement_sample_count"] > 0, f"{label} has no displacement samples")
    require(result["displacement_delta_rms_m"] > 0.0, f"{label} has no displacement RMS")
    require(result["object_zone_max_slope"] > 0.0, f"{label} has no object-zone slope")
    return result

baseline = read_profile("baseline", baseline_grid, baseline_frames_text, baseline_steps_text)
quality = read_profile("quality", quality_grid, quality_frames_text, quality_steps_text)

require(quality["sample_count"] > baseline["sample_count"],
        "quality profile did not increase surface sample count")
require(quality["object_footprint_columns"] > baseline["object_footprint_columns"],
        "quality profile did not increase object footprint resolution")
require(quality["displacement_sample_count"] > baseline["displacement_sample_count"],
        "quality profile did not increase displacement support samples")
require(quality["sample_spacing_x"] < baseline["sample_spacing_x"],
        "quality profile did not reduce sample spacing")
require(quality["object_zone_height_stddev_m"] <= max_object_zone_stddev_m,
        f"quality object-zone stddev too high: {quality['object_zone_height_stddev_m']}")
require(quality["object_zone_max_slope"] <= max_object_zone_slope,
        f"quality object-zone max slope too high: {quality['object_zone_max_slope']}")
require(quality["displacement_capped_sample_count"] == 0,
        f"quality profile capped displacement samples: {quality['displacement_capped_sample_count']}")

def ratio(a, b):
    return a / b if b else None

comparison = {
    "schema": "physics_sim_water_object_quality_compare_v1",
    "purpose": "WTR-6.5 PhysicsSim-only baseline-vs-quality diagnostics comparison",
    "profiles": {
        "baseline": baseline,
        "quality": quality,
    },
    "deltas": {
        "sample_count": quality["sample_count"] - baseline["sample_count"],
        "object_footprint_columns": quality["object_footprint_columns"] - baseline["object_footprint_columns"],
        "displacement_sample_count": quality["displacement_sample_count"] - baseline["displacement_sample_count"],
        "displacement_delta_rms_m": quality["displacement_delta_rms_m"] - baseline["displacement_delta_rms_m"],
        "object_zone_height_stddev_m": quality["object_zone_height_stddev_m"] - baseline["object_zone_height_stddev_m"],
        "object_zone_max_slope": quality["object_zone_max_slope"] - baseline["object_zone_max_slope"],
        "displacement_capped_sample_count": quality["displacement_capped_sample_count"] - baseline["displacement_capped_sample_count"],
    },
    "ratios": {
        "sample_count": ratio(quality["sample_count"], baseline["sample_count"]),
        "object_footprint_columns": ratio(quality["object_footprint_columns"], baseline["object_footprint_columns"]),
        "displacement_sample_count": ratio(quality["displacement_sample_count"], baseline["displacement_sample_count"]),
        "displacement_delta_rms_m": ratio(quality["displacement_delta_rms_m"], baseline["displacement_delta_rms_m"]),
        "object_zone_height_stddev_m": ratio(quality["object_zone_height_stddev_m"], baseline["object_zone_height_stddev_m"]),
        "object_zone_max_slope": ratio(quality["object_zone_max_slope"], baseline["object_zone_max_slope"]),
    },
    "interpretation": {
        "quality_increased_resolution": True,
        "quality_metric_improvement_required": True,
        "quality_max_object_zone_stddev_m": max_object_zone_stddev_m,
        "quality_max_object_zone_slope": max_object_zone_slope,
        "quality_passed_smoothing_bounds": True,
        "notes": [
            "This gate compares diagnostic shape and enforces bounded object-zone roughness.",
            "Object-zone slope is measured on wet local stencils so dry/solid holes do not dominate the smoothness score.",
        ],
    },
}

with open(summary_json_path, "w", encoding="utf-8") as f:
    json.dump(comparison, f, indent=2, sort_keys=True)
    f.write("\n")

lines = [
    "WTR-6.5 water object baseline-vs-quality comparison",
    f"baseline: grid={baseline['grid']} frames={baseline['frames']} steps={baseline['sim_steps_per_frame']}",
    f"quality:  grid={quality['grid']} frames={quality['frames']} steps={quality['sim_steps_per_frame']}",
    f"surface samples: {baseline['sample_count']} -> {quality['sample_count']}",
    f"footprint columns: {baseline['object_footprint_columns']} -> {quality['object_footprint_columns']}",
    f"displacement samples: {baseline['displacement_sample_count']} -> {quality['displacement_sample_count']}",
    f"displacement RMS m: {baseline['displacement_delta_rms_m']:.9f} -> {quality['displacement_delta_rms_m']:.9f}",
    f"object-zone stddev m: {baseline['object_zone_height_stddev_m']:.9f} -> {quality['object_zone_height_stddev_m']:.9f}",
    f"object-zone max slope: {baseline['object_zone_max_slope']:.9f} -> {quality['object_zone_max_slope']:.9f}",
    f"capped samples: {baseline['displacement_capped_sample_count']} -> {quality['displacement_capped_sample_count']}",
    f"quality thresholds: stddev<={max_object_zone_stddev_m:.6f} slope<={max_object_zone_slope:.6f}",
    f"summary: {summary_json_path}",
]
with open(summary_txt_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
    f.write("\n")

print(json.dumps({
    "summary": summary_json_path,
    "baseline": {
        "sample_count": baseline["sample_count"],
        "footprint_columns": baseline["object_footprint_columns"],
        "displacement_samples": baseline["displacement_sample_count"],
        "displacement_rms_m": baseline["displacement_delta_rms_m"],
        "object_zone_stddev_m": baseline["object_zone_height_stddev_m"],
        "object_zone_max_slope": baseline["object_zone_max_slope"],
    },
    "quality": {
        "sample_count": quality["sample_count"],
        "footprint_columns": quality["object_footprint_columns"],
        "displacement_samples": quality["displacement_sample_count"],
        "displacement_rms_m": quality["displacement_delta_rms_m"],
        "object_zone_stddev_m": quality["object_zone_height_stddev_m"],
        "object_zone_max_slope": quality["object_zone_max_slope"],
    },
}, indent=2))
PY

echo "physics_sim water object quality comparison passed: $SUMMARY_JSON"
