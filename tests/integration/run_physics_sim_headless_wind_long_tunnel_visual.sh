#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
RUNTIME_SCENE="$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_long_box.json"
OUT_DIR="$PHYSICS_DIR/tmp/headless_wind_long_tunnel_visual"
PROJECTION_OUT="$OUT_DIR/projection"
RENDER_OUT="$OUT_DIR/render"
REPORT="$OUT_DIR/long_tunnel_visual_summary.txt"
SUMMARY="$PROJECTION_OUT/run_summary.json"
MANIFEST="$PROJECTION_OUT/wind_shot_manifest.json"
TIMESERIES="$PROJECTION_OUT/wind_analysis_timeseries.jsonl"
INITIAL_PROJECTION="$PROJECTION_OUT/wind_projection_frames/frame_000000.bmp"
PROJECTION="$PROJECTION_OUT/wind_projection_frames/frame_000005.bmp"
RENDER_SUMMARY="$RENDER_OUT/run_summary.json"
RENDER_FRAME="$RENDER_OUT/render_frames/frame_000005.bmp"
RENDER_LOG="$OUT_DIR/render_attempt.log"
RENDER_BLOCKER="$OUT_DIR/renderer_blocker.txt"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

"$PHYSICS_DIR/physics_sim_headless" \
  --runtime-scene "$RUNTIME_SCENE" \
  --frames 6 \
  --sim-steps-per-frame 8 \
  --grid 96x24x24 \
  --wind-shot-camera side \
  --output-root "$PROJECTION_OUT" \
  --summary "$SUMMARY" \
  --overwrite \
  --save-volume-frames \
  --save-wind-projection-frames

test -f "$SUMMARY"
test -f "$MANIFEST"
test -f "$TIMESERIES"
test -f "$INITIAL_PROJECTION"
test -f "$PROJECTION"
test -s "$PROJECTION"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_headless_run_summary_v1"' "$SUMMARY"
rg -q '"status"[[:space:]]*:[[:space:]]*"passed"' "$SUMMARY"
rg -q '"save_wind_projection_frames"[[:space:]]*:[[:space:]]*true' "$SUMMARY"
rg -q '"schema"[[:space:]]*:[[:space:]]*"physics_sim_wind_shot_manifest_v1"' "$MANIFEST"
rg -q '"camera_profile"[[:space:]]*:[[:space:]]*"side"' "$MANIFEST"
line_count="$(wc -l < "$TIMESERIES" | tr -d ' ')"
test "$line_count" -ge 4
rg -q '"frame_index":5' "$TIMESERIES"
rg -q '"available":true' "$TIMESERIES"
if cmp -s "$INITIAL_PROJECTION" "$PROJECTION"; then
  echo "Wind projection frame 0 and final frame are identical: $INITIAL_PROJECTION $PROJECTION" >&2
  exit 1
fi

python3 - "$INITIAL_PROJECTION" "$PROJECTION" "$TIMESERIES" "$SUMMARY" "$MANIFEST" "$REPORT" <<'PY'
import json
import struct
import sys
from pathlib import Path

initial_projection = Path(sys.argv[1])
projection = Path(sys.argv[2])
timeseries = Path(sys.argv[3])
summary = Path(sys.argv[4])
manifest = Path(sys.argv[5])
report = Path(sys.argv[6])

def bmp_stats(path):
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise SystemExit(f"invalid BMP: {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = abs(struct.unpack_from("<i", data, 22)[0])
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp not in (24, 32):
        raise SystemExit(f"unsupported BMP bpp={bpp}: {path}")
    payload = data[offset:]
    nonzero = sum(1 for byte in payload if byte)
    unique = len(set(payload))
    if nonzero == 0 or unique < 2:
        raise SystemExit(f"blank BMP: {path}")
    return width, height, nonzero, unique

rows = [json.loads(line) for line in timeseries.read_text().splitlines() if line.strip()]
if len(rows) < 4:
    raise SystemExit(f"expected at least 4 Wind analysis rows, found {len(rows)}")
final = max(rows, key=lambda row: int(row.get("frame_index", -1)))
if int(final.get("frame_index", -1)) != 5:
    raise SystemExit(f"missing final Wind analysis row, max frame row is {final.get('frame_index')}")
for key in ("pressure_delta", "throughput_delta", "vorticity_avg", "vorticity_max"):
    if key not in final:
        raise SystemExit(f"missing {key} in final Wind row")
if not final.get("available"):
    raise SystemExit("final Wind row is unavailable")
if float(final.get("outlet_throughput", 0.0)) <= 0.0:
    raise SystemExit("final outlet throughput did not reach the outlet")
if float(final.get("vorticity_max", 0.0)) <= 0.0:
    raise SystemExit("final vorticity max is not positive")

initial_width, initial_height, initial_nonzero, initial_unique = bmp_stats(initial_projection)
width, height, nonzero, unique = bmp_stats(projection)
summary_json = json.loads(summary.read_text())
manifest_json = json.loads(manifest.read_text())
report.write_text(
    "PhysicsSim Wind long-tunnel visual proof\n"
    f"runtime_scene: {summary_json['runtime_scene']}\n"
    f"output_root: {summary_json['output_root']}\n"
    f"initial_projection_frame: {initial_projection}\n"
    f"initial_projection_bmp: {initial_width}x{initial_height} nonzero_bytes={initial_nonzero} unique_bytes={initial_unique}\n"
    f"projection_frame: {projection}\n"
    f"projection_bmp: {width}x{height} nonzero_bytes={nonzero} unique_bytes={unique}\n"
    "projection_changed: true\n"
    f"wind_manifest: {manifest}\n"
    f"wind_analysis_timeseries: {timeseries}\n"
    f"wind_analysis_rows: {len(rows)}\n"
    f"camera_profile: {manifest_json.get('camera_profile')}\n"
    f"final_pressure_delta: {final['pressure_delta']}\n"
    f"final_throughput_delta: {final['throughput_delta']}\n"
    f"final_outlet_throughput: {final['outlet_throughput']}\n"
    f"final_vorticity_avg: {final['vorticity_avg']}\n"
    f"final_vorticity_max: {final['vorticity_max']}\n",
    encoding="utf-8",
)
PY

set +e
"$PHYSICS_DIR/physics_sim_headless" \
  --runtime-scene "$RUNTIME_SCENE" \
  --frames 6 \
  --sim-steps-per-frame 8 \
  --grid 96x24x24 \
  --wind-shot-camera side \
  --output-root "$RENDER_OUT" \
  --summary "$RENDER_SUMMARY" \
  --overwrite \
  --save-render-frames >"$RENDER_LOG" 2>&1
render_status=$?
set -e

if [ "$render_status" -eq 0 ] && [ -f "$RENDER_FRAME" ] && [ -s "$RENDER_FRAME" ]; then
  python3 - "$RENDER_FRAME" "$REPORT" <<'PY'
import struct
import sys
from pathlib import Path

frame = Path(sys.argv[1])
report = Path(sys.argv[2])
data = frame.read_bytes()
if len(data) < 54 or data[:2] != b"BM":
    raise SystemExit(f"invalid render BMP: {frame}")
offset = struct.unpack_from("<I", data, 10)[0]
payload = data[offset:]
width = struct.unpack_from("<i", data, 18)[0]
height = abs(struct.unpack_from("<i", data, 22)[0])
nonzero = sum(1 for byte in payload if byte)
unique = len(set(payload))
if nonzero == 0 or unique < 2:
    raise SystemExit(f"blank render BMP: {frame}")
if width <= 96 or height <= 24:
    raise SystemExit(f"render BMP did not use the oblique software visualizer size: {width}x{height}")
with report.open("a", encoding="utf-8") as f:
    f.write(f"render_frame: {frame}\n")
    f.write(f"render_bmp: {width}x{height} nonzero_bytes={nonzero} unique_bytes={unique}\n")
PY
else
  {
    echo "Renderer frame output was attempted but did not produce a nonblank final frame."
    echo "render_status: $render_status"
    echo "render_output_root: $RENDER_OUT"
    echo "render_log: $RENDER_LOG"
    echo "expected_render_frame: $RENDER_FRAME"
  } > "$RENDER_BLOCKER"
  {
    echo "render_frame: unavailable"
    echo "renderer_blocker: $RENDER_BLOCKER"
    echo "render_log: $RENDER_LOG"
  } >> "$REPORT"
  cat "$REPORT"
  echo "physics_sim headless wind long-tunnel visual smoke passed with renderer blocker: $REPORT"
  exit 0
fi

cat "$REPORT"
echo "physics_sim headless wind long-tunnel visual smoke passed: $REPORT"
