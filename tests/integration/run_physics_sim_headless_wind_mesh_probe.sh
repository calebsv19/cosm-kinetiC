#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="${WIND_MESH_PROBE_OUT_DIR:-$PHYSICS_DIR/tmp/headless_wind_mesh_probe}"
REPORT="$OUT_DIR/mesh_probe_summary.txt"
JSON_SUMMARY="$OUT_DIR/mesh_probe_summary.json"
MODE="${WIND_MESH_PROBE_MODE:-volume_speed_deficit}"
FRAMES="${WIND_MESH_PROBE_FRAMES:-20}"
STEPS_PER_FRAME="${WIND_MESH_PROBE_SIM_STEPS_PER_FRAME:-2}"
GRID="${WIND_MESH_PROBE_GRID:-96x24x24}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

cases=(
  "mesh_wedge_wide:$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_mesh_wedge_wide.json"
  "mesh_wedge_slim:$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_mesh_wedge_slim.json"
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
    --save-render-frames \
    --save-wind-projection-frames >"$case_dir/headless.log" 2>&1
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
    scene_path = Path(scene)
    case_dir = out_dir / name
    timeseries = case_dir / "wind_analysis_timeseries.jsonl"
    summary = case_dir / "run_summary.json"
    render_frame = case_dir / "render_frames" / f"frame_{frames - 1:06d}.bmp"
    projection_frame = case_dir / "wind_projection_frames" / f"frame_{frames - 1:06d}.bmp"
    if not scene_path.is_file():
        raise SystemExit(f"{name}: missing runtime scene {scene_path}")
    scene_json = json.loads(scene_path.read_text(encoding="utf-8"))
    objects = scene_json.get("objects", [])
    if not any(obj.get("object_type") == "mesh_asset_instance" for obj in objects):
        raise SystemExit(f"{name}: scene does not contain a mesh_asset_instance")
    if not timeseries.is_file():
        raise SystemExit(f"{name}: missing {timeseries}")
    if not summary.is_file():
        raise SystemExit(f"{name}: missing {summary}")
    if not render_frame.is_file() or render_frame.stat().st_size <= 0:
        raise SystemExit(f"{name}: missing nonempty render frame {render_frame}")
    if not projection_frame.is_file() or projection_frame.stat().st_size <= 0:
        raise SystemExit(f"{name}: missing nonempty Wind projection frame {projection_frame}")
    rows = [json.loads(line) for line in timeseries.read_text(encoding="utf-8").splitlines() if line.strip()]
    if len(rows) < frames:
        raise SystemExit(f"{name}: expected at least {frames} Wind rows, found {len(rows)}")
    final = max(rows, key=lambda row: int(row.get("frame_index", -1)))
    if not final.get("available"):
        raise SystemExit(f"{name}: final Wind analysis row unavailable")
    if not final.get("object_drag_available"):
        raise SystemExit(f"{name}: object drag readout unavailable")
    solid_cells = float(final.get("object_solid_cells", 0.0))
    projected_area = float(final.get("object_projected_area", 0.0))
    drag_proxy = float(final.get("object_drag_pressure_proxy", 0.0))
    outlet_throughput = float(final.get("outlet_throughput", 0.0))
    if solid_cells <= 0.0:
        raise SystemExit(f"{name}: mesh did not contribute solid cells")
    if projected_area <= 0.0:
        raise SystemExit(f"{name}: mesh projected area not positive")
    if abs(drag_proxy) <= 1.0e-6:
        raise SystemExit(f"{name}: mesh drag proxy too small")
    if outlet_throughput <= 0.0:
        raise SystemExit(f"{name}: outlet throughput not positive")
    results.append({
        "name": name,
        "runtime_scene": str(scene_path),
        "output_root": str(case_dir),
        "render_frame": str(render_frame),
        "wind_projection_frame": str(projection_frame),
        "final_frame_index": int(final["frame_index"]),
        "object_solid_cells": final["object_solid_cells"],
        "object_projected_area": final["object_projected_area"],
        "object_pressure_delta": final["object_pressure_delta"],
        "object_drag_pressure_proxy": final["object_drag_pressure_proxy"],
        "pressure_delta": final["pressure_delta"],
        "throughput_delta": final["throughput_delta"],
        "outlet_throughput": final["outlet_throughput"],
        "vorticity_avg": final["vorticity_avg"],
        "vorticity_max": final["vorticity_max"],
    })

by_name = {row["name"]: row for row in results}
wide = by_name["mesh_wedge_wide"]
slim = by_name["mesh_wedge_slim"]
wide_area = float(wide["object_projected_area"])
slim_area = float(slim["object_projected_area"])
wide_drag = float(wide["object_drag_pressure_proxy"])
slim_drag = float(slim["object_drag_pressure_proxy"])
if wide_area <= slim_area:
    raise SystemExit(f"mesh projected area ordering failed: wide={wide_area} slim={slim_area}")
if abs(wide_drag - slim_drag) <= 1.0e-4:
    raise SystemExit(f"mesh drag proxy did not differ: wide={wide_drag} slim={slim_drag}")

payload = {
    "schema": "physics_sim_wind_mesh_probe_v1",
    "output_root": str(out_dir),
    "grid": grid,
    "frames": frames,
    "sim_steps_per_frame": steps_per_frame,
    "wind_visual_mode": mode,
    "results": results,
}
json_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

lines = [
    "PhysicsSim Wind mesh probe",
    f"output_root: {out_dir}",
    f"grid: {grid}",
    f"frames: {frames}",
    f"sim_steps_per_frame: {steps_per_frame}",
    f"wind_visual_mode: {mode}",
    "",
    "mesh,solid_cells,projected_area,object_pressure_delta,object_drag_pressure_proxy,pressure_delta,throughput_delta,vorticity_avg,vorticity_max,render_frame,wind_projection_frame",
]
for row in results:
    lines.append(
        f"{row['name']},{row['object_solid_cells']},{row['object_projected_area']},"
        f"{row['object_pressure_delta']},{row['object_drag_pressure_proxy']},"
        f"{row['pressure_delta']},{row['throughput_delta']},"
        f"{row['vorticity_avg']},{row['vorticity_max']},"
        f"{row['render_frame']},{row['wind_projection_frame']}"
    )
lines.extend([
    "",
    f"json_summary: {json_path}",
])
report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
PY

cat "$REPORT"
echo "physics_sim headless Wind mesh probe passed: $REPORT"
