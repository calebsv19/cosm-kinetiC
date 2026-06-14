#!/usr/bin/env bash
set -euo pipefail

PHYSICS_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
RUNTIME_SCENE="${WIND_VIDEO_RUNTIME_SCENE:-$PHYSICS_DIR/tests/fixtures/runtime_scene_wind_tunnel_3d_long_box.json}"
OUT_DIR="${WIND_VIDEO_OUT_DIR:-$PHYSICS_DIR/tmp/headless_wind_long_tunnel_video}"
SUMMARY="$OUT_DIR/run_summary.json"
PROGRESS="$OUT_DIR/run_progress.json"
TIMESERIES="$OUT_DIR/wind_analysis_timeseries.jsonl"
FRAME_DIR="$OUT_DIR/render_frames"
MP4="$OUT_DIR/wind_long_tunnel_oblique.mp4"
REPORT="$OUT_DIR/wind_long_tunnel_video_summary.txt"
LOG="$OUT_DIR/headless_render.log"
FFMPEG_LOG="$OUT_DIR/ffmpeg.log"

QUALITY="${WIND_VIDEO_QUALITY:-high}"
case "$QUALITY" in
  smoke)
    DEFAULT_FRAMES=36
    DEFAULT_STEPS_PER_FRAME=2
    DEFAULT_FPS=12
    DEFAULT_GRID="120x32x32"
    ;;
  high)
    DEFAULT_FRAMES=144
    DEFAULT_STEPS_PER_FRAME=3
    DEFAULT_FPS=18
    DEFAULT_GRID="144x36x36"
    ;;
  *)
    echo "unsupported WIND_VIDEO_QUALITY '$QUALITY' (expected smoke or high)" >&2
    exit 1
    ;;
esac

FRAMES="${WIND_VIDEO_FRAMES:-$DEFAULT_FRAMES}"
STEPS_PER_FRAME="${WIND_VIDEO_SIM_STEPS_PER_FRAME:-$DEFAULT_STEPS_PER_FRAME}"
FPS="${WIND_VIDEO_FPS:-$DEFAULT_FPS}"
GRID="${WIND_VIDEO_GRID:-$DEFAULT_GRID}"
KEEP_BMPS="${WIND_VIDEO_KEEP_BMPS:-0}"
MODE="${WIND_VIDEO_MODE:-volume_vorticity}"

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg is required to encode the Wind tunnel MP4" >&2
  exit 1
fi

if ! command -v ffprobe >/dev/null 2>&1; then
  echo "ffprobe is required to validate the Wind tunnel MP4" >&2
  exit 1
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

"$PHYSICS_DIR/physics_sim_headless" \
  --runtime-scene "$RUNTIME_SCENE" \
  --frames "$FRAMES" \
  --sim-steps-per-frame "$STEPS_PER_FRAME" \
  --grid "$GRID" \
  --wind-shot-camera side \
  --wind-visual-mode "$MODE" \
  --output-root "$OUT_DIR" \
  --summary "$SUMMARY" \
  --progress "$PROGRESS" \
  --overwrite \
  --save-render-frames >"$LOG" 2>&1

test -f "$SUMMARY"
test -f "$TIMESERIES"
test -d "$FRAME_DIR"
rg -q '"status"[[:space:]]*:[[:space:]]*"passed"' "$SUMMARY"
rg -q '"available":true' "$TIMESERIES"

frame_count="$(find "$FRAME_DIR" -name 'frame_*.bmp' -type f | wc -l | tr -d ' ')"
if [ "$frame_count" -lt "$FRAMES" ]; then
  echo "expected at least $FRAMES BMP frames, found $frame_count in $FRAME_DIR" >&2
  exit 1
fi

ffmpeg \
  -y \
  -framerate "$FPS" \
  -i "$FRAME_DIR/frame_%06d.bmp" \
  -vf format=yuv420p \
  -movflags +faststart \
  "$MP4" >"$FFMPEG_LOG" 2>&1

test -s "$MP4"

python3 - "$SUMMARY" "$TIMESERIES" "$MP4" "$REPORT" "$FRAMES" "$FPS" "$STEPS_PER_FRAME" "$GRID" "$frame_count" "$MODE" "$FRAME_DIR" "$QUALITY" <<'PY'
import json
import sys
from pathlib import Path

summary = Path(sys.argv[1])
timeseries = Path(sys.argv[2])
mp4 = Path(sys.argv[3])
report = Path(sys.argv[4])
frames = int(sys.argv[5])
fps = float(sys.argv[6])
steps_per_frame = int(sys.argv[7])
grid = sys.argv[8]
frame_count = int(sys.argv[9])
mode = sys.argv[10]
frame_dir = Path(sys.argv[11])
quality = sys.argv[12]

rows = [json.loads(line) for line in timeseries.read_text().splitlines() if line.strip()]
if len(rows) < frames:
    raise SystemExit(f"expected at least {frames} Wind analysis rows, found {len(rows)}")
final = max(rows, key=lambda row: int(row.get("frame_index", -1)))
if not final.get("available"):
    raise SystemExit("final Wind analysis row is unavailable")
if float(final.get("outlet_throughput", 0.0)) <= 0.0:
    raise SystemExit("final outlet throughput did not reach the outlet")
if float(final.get("vorticity_max", 0.0)) <= 0.0:
    raise SystemExit("final vorticity max is not positive")

first_frame = frame_dir / "frame_000000.bmp"
last_frame = frame_dir / f"frame_{frames - 1:06d}.bmp"
if not first_frame.is_file() or not last_frame.is_file():
    raise SystemExit("first/final render BMP frames are missing")
first_bytes = first_frame.read_bytes()
last_bytes = last_frame.read_bytes()
changed_bytes = sum(1 for a, b in zip(first_bytes, last_bytes) if a != b)
changed_bytes += abs(len(first_bytes) - len(last_bytes))
if changed_bytes <= 0:
    raise SystemExit("first and final render BMP frames are identical")

summary_json = json.loads(summary.read_text())
report.write_text(
    "PhysicsSim Wind long-tunnel MP4 proof\n"
    f"runtime_scene: {summary_json['runtime_scene']}\n"
    f"output_root: {summary_json['output_root']}\n"
    f"quality: {quality}\n"
    f"grid: {grid}\n"
    f"wind_visual_mode: {mode}\n"
    f"frames: {frames}\n"
    f"sim_steps_per_frame: {steps_per_frame}\n"
    f"fps: {fps:g}\n"
    f"encoded_frame_count: {frame_count}\n"
    f"mp4: {mp4}\n"
    f"mp4_bytes: {mp4.stat().st_size}\n"
    f"render_frames_changed: true\n"
    f"render_frame_changed_bytes: {changed_bytes}\n"
    f"wind_analysis_rows: {len(rows)}\n"
    f"final_frame_index: {final['frame_index']}\n"
    f"final_pressure_delta: {final['pressure_delta']}\n"
    f"final_throughput_delta: {final['throughput_delta']}\n"
    f"final_outlet_throughput: {final['outlet_throughput']}\n"
    f"final_vorticity_avg: {final['vorticity_avg']}\n"
    f"final_vorticity_max: {final['vorticity_max']}\n",
    encoding="utf-8",
)
PY

duration="$(ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 "$MP4")"
video_streams="$(ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,nb_frames -of csv=p=0 "$MP4")"
{
  echo "mp4_duration_seconds: $duration"
  echo "mp4_video_stream: $video_streams"
} >> "$REPORT"

if [ "$KEEP_BMPS" != "1" ]; then
  rm -rf "$FRAME_DIR"
  echo "bmp_frames_removed: true" >> "$REPORT"
else
  echo "bmp_frames_removed: false" >> "$REPORT"
  echo "bmp_frame_dir: $FRAME_DIR" >> "$REPORT"
fi

cat "$REPORT"
echo "physics_sim headless Wind long-tunnel MP4 smoke passed: $MP4"
