#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
RUNTIME_SCENE="$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_minimal.json"
ARTIFACT_ROOT="${PHYSICS_SIM_VISUAL_ARTIFACT_DIR:-$PHYSICS_DIR/visual_artifacts/source_first_frame}"
RUN_DIR="$ARTIFACT_ROOT/run"
SUMMARY="$RUN_DIR/run_summary.json"
MANIFEST="$RUN_DIR/wind_shot_manifest.json"
TIMESERIES="$RUN_DIR/wind_analysis_timeseries.jsonl"
SOURCE_FRAME="$RUN_DIR/wind_projection_frames/frame_000000.bmp"
ARTIFACT="$ARTIFACT_ROOT/physics_sim_wind_projection_first_frame.bmp"
REPORT="$ARTIFACT_ROOT/visual_artifact_report.json"

rm -rf "$ARTIFACT_ROOT"
mkdir -p "$RUN_DIR"

"$PHYSICS_DIR/physics_sim_headless" \
  --runtime-scene "$RUNTIME_SCENE" \
  --frames 1 \
  --sim-steps-per-frame 4 \
  --grid 64x32x32 \
  --wind-shot-camera side \
  --output-root "$RUN_DIR" \
  --summary "$SUMMARY" \
  --overwrite \
  --save-volume-frames \
  --save-wind-projection-frames

test -f "$SUMMARY"
test -f "$MANIFEST"
test -f "$TIMESERIES"
test -f "$SOURCE_FRAME"
test -s "$SOURCE_FRAME"

python3 - "$SUMMARY" "$MANIFEST" "$TIMESERIES" "$SOURCE_FRAME" "$ARTIFACT" "$REPORT" <<'PY'
import json
import shutil
import struct
import sys
from pathlib import Path

summary_path = Path(sys.argv[1])
manifest_path = Path(sys.argv[2])
timeseries_path = Path(sys.argv[3])
source_frame = Path(sys.argv[4])
artifact = Path(sys.argv[5])
report_path = Path(sys.argv[6])


def bmp_stats(path):
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise SystemExit(f"invalid BMP artifact: {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = abs(struct.unpack_from("<i", data, 22)[0])
    bpp = struct.unpack_from("<H", data, 28)[0]
    if width <= 0 or height <= 0:
        raise SystemExit(f"invalid BMP dimensions: {path}")
    if bpp not in (24, 32):
        raise SystemExit(f"unsupported BMP bits-per-pixel={bpp}: {path}")
    payload = data[offset:]
    nonzero_bytes = sum(1 for value in payload if value)
    unique_bytes = len(set(payload))
    if nonzero_bytes == 0 or unique_bytes < 2:
        raise SystemExit(f"blank BMP artifact: {path}")
    return {
        "width": width,
        "height": height,
        "bits_per_pixel": bpp,
        "pixel_payload_bytes": len(payload),
        "nonzero_payload_bytes": nonzero_bytes,
        "unique_payload_byte_values": unique_bytes,
    }


summary = json.loads(summary_path.read_text(encoding="utf-8"))
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
rows = [
    json.loads(line)
    for line in timeseries_path.read_text(encoding="utf-8").splitlines()
    if line.strip()
]
if summary.get("status") != "passed":
    raise SystemExit("headless run did not pass")
if manifest.get("schema") != "physics_sim_wind_shot_manifest_v1":
    raise SystemExit("unexpected Wind shot manifest schema")
if not rows:
    raise SystemExit("missing Wind analysis rows")
if not rows[-1].get("available"):
    raise SystemExit("Wind analysis row is unavailable")

stats = bmp_stats(source_frame)
artifact.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(source_frame, artifact)

report = {
    "schema": "physics_sim_visual_artifact_report_v1",
    "program": "physics_sim",
    "product": "kinetiC",
    "proof": "source_run_first_frame_wind_projection",
    "runtime_scene": summary.get("runtime_scene"),
    "output_root": summary.get("output_root"),
    "artifact_path": str(artifact),
    "source_frame": str(source_frame),
    "summary": str(summary_path),
    "wind_manifest": str(manifest_path),
    "wind_analysis_timeseries": str(timeseries_path),
    "wind_analysis_rows": len(rows),
    "camera_profile": manifest.get("camera_profile"),
    "frame_index": 0,
    "bmp": stats,
}
report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
PY

test -f "$ARTIFACT"
test -s "$ARTIFACT"
test -f "$REPORT"

echo "physics_sim visual artifact: $ARTIFACT"
