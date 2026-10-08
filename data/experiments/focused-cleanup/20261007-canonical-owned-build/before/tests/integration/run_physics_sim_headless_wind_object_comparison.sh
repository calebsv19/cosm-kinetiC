#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="${WIND_OBJECT_COMPARISON_OUT_DIR:-$PHYSICS_DIR/tmp/headless_wind_object_comparison}"
REPORT="$OUT_DIR/object_comparison_summary.txt"
JSON_SUMMARY="$OUT_DIR/object_comparison_summary.json"
MODE="${WIND_OBJECT_COMPARISON_MODE:-volume_speed_deficit}"
FRAMES="${WIND_OBJECT_COMPARISON_FRAMES:-24}"
STEPS_PER_FRAME="${WIND_OBJECT_COMPARISON_SIM_STEPS_PER_FRAME:-2}"
GRID="${WIND_OBJECT_COMPARISON_GRID:-96x24x24}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

cases=(
  "blunt_box:$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_long_box.json"
  "sphere:$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_long_sphere.json"
  "slim_box:$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_long_slim_box.json"
)

for entry in "${cases[@]}"; do
  name="${entry%%:*}"
  scene="${entry#*:}"
  case_dir="$OUT_DIR/$name"
  mkdir -p "$case_dir"
  "$PHYSICS_DIR/physics_sim_headless" \
    --runtime-scene "$scene" \
    --frames "$FRAMES" \
    --sim-steps-per-frame "$STEPS_PER_FRAME" \
    --grid "$GRID" \
    --wind-shot-camera side \
    --wind-visual-mode "$MODE" \
    --output-root "$case_dir" \
    --summary "$case_dir/run_summary.json" \
    --progress "$case_dir/run_progress.json" \
    --overwrite \
    --save-render-frames >"$case_dir/headless.log" 2>&1
done

python3 - "$OUT_DIR" "$REPORT" "$JSON_SUMMARY" "$FRAMES" "$STEPS_PER_FRAME" "$GRID" "$MODE" "${cases[@]}" <<'PY'
import json
import sys
from pathlib import Path

out_dir = Path(sys.argv[1])
report_path = Path(sys.argv[2])
json_path = Path(sys.argv[3])
frames = int(sys.argv[4])
steps_per_frame = int(sys.argv[5])
grid = sys.argv[6]
mode = sys.argv[7]
case_entries = sys.argv[8:]

results = []
for entry in case_entries:
    name, scene = entry.split(":", 1)
    case_dir = out_dir / name
    timeseries = case_dir / "wind_analysis_timeseries.jsonl"
    summary = case_dir / "run_summary.json"
    frame_dir = case_dir / "render_frames"
    final_frame = frame_dir / f"frame_{frames - 1:06d}.bmp"
    first_frame = frame_dir / "frame_000000.bmp"
    if not timeseries.is_file():
        raise SystemExit(f"{name}: missing {timeseries}")
    if not summary.is_file():
        raise SystemExit(f"{name}: missing {summary}")
    if not final_frame.is_file():
        raise SystemExit(f"{name}: missing final render frame {final_frame}")
    rows = [json.loads(line) for line in timeseries.read_text().splitlines() if line.strip()]
    if len(rows) < frames:
        raise SystemExit(f"{name}: expected at least {frames} analysis rows, found {len(rows)}")
    final = max(rows, key=lambda row: int(row.get("frame_index", -1)))
    if not final.get("available"):
        raise SystemExit(f"{name}: final Wind analysis row unavailable")
    if not final.get("object_drag_available"):
        raise SystemExit(f"{name}: object drag readout unavailable")
    if float(final.get("object_solid_cells", 0.0)) <= 0.0:
        raise SystemExit(f"{name}: object solid cell count not positive")
    if float(final.get("object_projected_area", 0.0)) <= 0.0:
        raise SystemExit(f"{name}: object projected area not positive")
    if float(final.get("outlet_throughput", 0.0)) <= 0.0:
        raise SystemExit(f"{name}: outlet throughput not positive")
    changed_bytes = None
    if first_frame.is_file():
        first = first_frame.read_bytes()
        last = final_frame.read_bytes()
        changed_bytes = sum(1 for a, b in zip(first, last) if a != b) + abs(len(first) - len(last))
        if changed_bytes <= 0:
            raise SystemExit(f"{name}: first/final render frames are identical")
    results.append({
        "name": name,
        "runtime_scene": scene,
        "output_root": str(case_dir),
        "render_frame": str(final_frame),
        "frame_changed_bytes": changed_bytes,
        "final_frame_index": final["frame_index"],
        "pressure_delta": final["pressure_delta"],
        "throughput_delta": final["throughput_delta"],
        "outlet_throughput": final["outlet_throughput"],
        "vorticity_avg": final["vorticity_avg"],
        "vorticity_max": final["vorticity_max"],
        "object_solid_cells": final["object_solid_cells"],
        "object_projected_area": final["object_projected_area"],
        "object_pressure_delta": final["object_pressure_delta"],
        "object_drag_pressure_proxy": final["object_drag_pressure_proxy"],
    })

drag_values = [float(row["object_drag_pressure_proxy"]) for row in results]
area_values = [float(row["object_projected_area"]) for row in results]
if max(drag_values) - min(drag_values) <= 1.0e-4:
    raise SystemExit("object comparison did not produce distinct drag-pressure proxy values")
if max(area_values) - min(area_values) <= 1.0e-4:
    raise SystemExit("object comparison did not produce distinct projected areas")

payload = {
    "schema": "physics_sim_wind_object_comparison_v1",
    "output_root": str(out_dir),
    "grid": grid,
    "frames": frames,
    "sim_steps_per_frame": steps_per_frame,
    "wind_visual_mode": mode,
    "results": results,
}
json_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

lines = [
    "PhysicsSim Wind object comparison",
    f"output_root: {out_dir}",
    f"grid: {grid}",
    f"frames: {frames}",
    f"sim_steps_per_frame: {steps_per_frame}",
    f"wind_visual_mode: {mode}",
    "",
    "shape,solid_cells,projected_area,object_pressure_delta,object_drag_pressure_proxy,pressure_delta,throughput_delta,vorticity_avg,vorticity_max,render_frame",
]
for row in results:
    lines.append(
        f"{row['name']},{row['object_solid_cells']},{row['object_projected_area']},"
        f"{row['object_pressure_delta']},{row['object_drag_pressure_proxy']},"
        f"{row['pressure_delta']},{row['throughput_delta']},"
        f"{row['vorticity_avg']},{row['vorticity_max']},{row['render_frame']}"
    )
lines.extend([
    "",
    f"json_summary: {json_path}",
])
report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
PY

cat "$REPORT"
echo "physics_sim headless Wind object comparison passed: $REPORT"
