#!/usr/bin/env python3
"""Duplicate a Wind runtime scene, rotate one object, and compare headless metrics."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


DEFAULT_ORIENTATIONS = (
    "baseline:0,0,0",
    "roll45:0,0,45",
    "roll90:0,0,90",
)

ORIENTATION_PRESETS = {
    "roll-sweep": DEFAULT_ORIENTATIONS,
    "dragonwind-roll": (
        "authored:0,0,0",
        "roll45:0,0,45",
        "roll90:0,0,90",
    ),
    "axis-check": (
        "baseline:0,0,0",
        "pitch45:45,0,0",
        "yaw45:0,45,0",
        "roll45:0,0,45",
    ),
}


def parse_orientation(value: str) -> tuple[str, dict[str, float]]:
    if ":" not in value:
        raise argparse.ArgumentTypeError(
            f"orientation must be name:x,y,z, got {value!r}"
        )
    name, raw_xyz = value.split(":", 1)
    name = name.strip()
    if not name:
        raise argparse.ArgumentTypeError("orientation name must not be empty")
    parts = [part.strip() for part in raw_xyz.split(",")]
    if len(parts) != 3:
        raise argparse.ArgumentTypeError(
            f"orientation {name!r} must provide x,y,z degrees"
        )
    try:
        x, y, z = (float(part) for part in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            f"orientation {name!r} rotation values must be numeric"
        ) from exc
    return name, {"x": x, "y": y, "z": z}


def safe_case_name(name: str) -> str:
    out = []
    for ch in name:
        if ch.isalnum() or ch in ("-", "_"):
            out.append(ch)
        else:
            out.append("_")
    safe = "".join(out).strip("_")
    return safe or "orientation"


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise SystemExit(f"{path}: runtime scene root must be an object")
    return data


def object_type_supported_for_probe(obj: dict[str, Any]) -> bool:
    return obj.get("object_type") in ("mesh_asset_instance", "box", "sphere")


def object_rotation(obj: dict[str, Any]) -> dict[str, float]:
    transform = obj.get("transform")
    if not isinstance(transform, dict):
        return {"x": 0.0, "y": 0.0, "z": 0.0}
    rotation = transform.get("rotation")
    if not isinstance(rotation, dict):
        return {"x": 0.0, "y": 0.0, "z": 0.0}
    return {
        "x": float(rotation.get("x", 0.0)),
        "y": float(rotation.get("y", 0.0)),
        "z": float(rotation.get("z", 0.0)),
    }


def discover_probe_objects(scene: dict[str, Any]) -> list[dict[str, Any]]:
    objects = scene.get("objects")
    if not isinstance(objects, list):
        return []
    candidates = []
    for index, obj in enumerate(objects):
        if not isinstance(obj, dict):
            continue
        transform = obj.get("transform")
        extensions = obj.get("extensions")
        physics_sim = extensions.get("physics_sim") if isinstance(extensions, dict) else None
        line_drawing = extensions.get("line_drawing") if isinstance(extensions, dict) else None
        candidate = {
            "index": index,
            "object_id": str(obj.get("object_id", "")),
            "object_type": str(obj.get("object_type", "")),
            "supported": object_type_supported_for_probe(obj),
            "rotation_deg": object_rotation(obj),
            "has_transform": isinstance(transform, dict),
            "fluid_behavior": None,
            "runtime_mesh_path": None,
        }
        if isinstance(physics_sim, dict):
            behavior = physics_sim.get("fluid_behavior")
            if isinstance(behavior, str):
                candidate["fluid_behavior"] = behavior
        if isinstance(line_drawing, dict):
            mesh_path = line_drawing.get("runtime_mesh_path")
            if isinstance(mesh_path, str):
                candidate["runtime_mesh_path"] = mesh_path
        candidates.append(candidate)
    return candidates


def format_object_listing(candidates: list[dict[str, Any]]) -> str:
    lines = ["Probe object candidates:"]
    if not candidates:
        lines.append("  none")
        return "\n".join(lines)
    for item in candidates:
        rotation = item["rotation_deg"]
        marker = "*" if item["supported"] else "-"
        parts = [
            f"  {marker} index={item['index']}",
            f"id={item['object_id'] or '<missing>'}",
            f"type={item['object_type'] or '<missing>'}",
            f"rotation={rotation['x']}/{rotation['y']}/{rotation['z']}",
        ]
        if item["fluid_behavior"]:
            parts.append(f"fluid_behavior={item['fluid_behavior']}")
        if item["runtime_mesh_path"]:
            parts.append(f"runtime_mesh_path={item['runtime_mesh_path']}")
        lines.append(" ".join(parts))
    lines.append("* = default eligible for orientation probing")
    return "\n".join(lines)


def find_target_object(scene: dict[str, Any], object_id: str | None) -> dict[str, Any]:
    objects = scene.get("objects")
    if not isinstance(objects, list) or not objects:
        raise SystemExit("runtime scene has no objects; nothing can be orientation-probed")

    if object_id:
        for obj in objects:
            if isinstance(obj, dict) and obj.get("object_id") == object_id:
                if not object_type_supported_for_probe(obj):
                    raise SystemExit(
                        f"object_id {object_id!r} has type {obj.get('object_type')!r}; "
                        "expected mesh_asset_instance, box, or sphere"
                    )
                return obj
        raise SystemExit(
            f"object_id {object_id!r} not found\n"
            f"{format_object_listing(discover_probe_objects(scene))}"
        )

    for obj in objects:
        if not isinstance(obj, dict):
            continue
        if object_type_supported_for_probe(obj):
            return obj
    raise SystemExit(
        "runtime scene has no supported orientation-probe object "
        "(mesh_asset_instance, box, or sphere)\n"
        f"{format_object_listing(discover_probe_objects(scene))}"
    )


def require_wind_tunnel(scene: dict[str, Any], path: Path) -> None:
    extensions = scene.get("extensions")
    physics_sim = extensions.get("physics_sim") if isinstance(extensions, dict) else None
    wind_tunnel = physics_sim.get("wind_tunnel") if isinstance(physics_sim, dict) else None
    if not isinstance(wind_tunnel, dict) or not wind_tunnel.get("active"):
        raise SystemExit(
            f"{path}: missing active extensions.physics_sim.wind_tunnel; "
            "persist Wind setup before running orientation probes"
        )


def absolutize_runtime_mesh_paths(scene: dict[str, Any], source_dir: Path) -> None:
    objects = scene.get("objects", [])
    if not isinstance(objects, list):
        return
    for obj in objects:
        if not isinstance(obj, dict):
            continue
        extensions = obj.get("extensions")
        if not isinstance(extensions, dict):
            continue
        line_drawing = extensions.get("line_drawing")
        if not isinstance(line_drawing, dict):
            continue
        mesh_path = line_drawing.get("runtime_mesh_path")
        if not isinstance(mesh_path, str) or not mesh_path:
            continue
        path = Path(mesh_path)
        if path.is_absolute():
            continue
        line_drawing["runtime_mesh_path"] = str((source_dir / path).resolve())


def write_oriented_scene(
    source_scene: dict[str, Any],
    source_path: Path,
    output_path: Path,
    object_id: str | None,
    orientation_name: str,
    rotation: dict[str, float],
    rotation_mode: str,
) -> tuple[str, dict[str, float]]:
    scene = json.loads(json.dumps(source_scene))
    absolutize_runtime_mesh_paths(scene, source_path.parent)
    target = find_target_object(scene, object_id)
    transform = target.setdefault("transform", {})
    if not isinstance(transform, dict):
        transform = {}
        target["transform"] = transform
    if rotation_mode == "relative":
        source_rotation = transform.get("rotation")
        if not isinstance(source_rotation, dict):
            source_rotation = {}
        applied_rotation = {
            "x": float(source_rotation.get("x", 0.0)) + rotation["x"],
            "y": float(source_rotation.get("y", 0.0)) + rotation["y"],
            "z": float(source_rotation.get("z", 0.0)) + rotation["z"],
        }
    else:
        applied_rotation = rotation
    transform["rotation"] = applied_rotation
    base_scene_id = scene.get("scene_id")
    if isinstance(base_scene_id, str) and base_scene_id:
        scene["scene_id"] = f"{base_scene_id}_{safe_case_name(orientation_name)}"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(scene, indent=2) + "\n", encoding="utf-8")
    return str(target.get("object_id", "")), applied_rotation


def run_headless(args: argparse.Namespace, scene_path: Path, case_dir: Path) -> None:
    case_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(args.headless_bin),
        "--runtime-scene",
        str(scene_path),
        "--frames",
        str(args.frames),
        "--sim-steps-per-frame",
        str(args.sim_steps_per_frame),
        "--grid",
        args.grid,
        "--wind-shot-camera",
        args.wind_shot_camera,
        "--wind-visual-mode",
        args.wind_visual_mode,
        "--output-root",
        str(case_dir),
        "--summary",
        str(case_dir / "run_summary.json"),
        "--progress",
        str(case_dir / "run_progress.json"),
        "--overwrite",
        "--save-render-frames",
        "--save-wind-projection-frames",
    ]
    log_path = case_dir / "headless.log"
    with log_path.open("w", encoding="utf-8") as log:
        result = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=False)
    if result.returncode != 0:
        raise SystemExit(f"headless failed for {scene_path}; see {log_path}")


def read_final_metrics(case_dir: Path, frames: int) -> dict[str, Any]:
    timeseries = case_dir / "wind_analysis_timeseries.jsonl"
    if not timeseries.is_file():
        raise SystemExit(f"missing Wind analysis timeseries: {timeseries}")
    rows = [
        json.loads(line)
        for line in timeseries.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    if not rows:
        raise SystemExit(f"empty Wind analysis timeseries: {timeseries}")
    final = max(rows, key=lambda row: int(row.get("frame_index", -1)))
    if not final.get("available"):
        raise SystemExit(f"final Wind analysis row unavailable in {timeseries}")
    if not final.get("object_drag_available"):
        raise SystemExit(f"object drag readout unavailable in {timeseries}")

    render_frame = case_dir / "render_frames" / f"frame_{frames - 1:06d}.bmp"
    projection_frame = case_dir / "wind_projection_frames" / f"frame_{frames - 1:06d}.bmp"
    if not render_frame.is_file() or render_frame.stat().st_size <= 0:
        raise SystemExit(f"missing nonempty final render frame: {render_frame}")
    if not projection_frame.is_file() or projection_frame.stat().st_size <= 0:
        raise SystemExit(f"missing nonempty final Wind projection frame: {projection_frame}")

    metrics = {
        "final_frame_index": int(final.get("frame_index", -1)),
        "object_solid_cells": final.get("object_solid_cells"),
        "object_projected_area": final.get("object_projected_area"),
        "object_pressure_delta": final.get("object_pressure_delta"),
        "object_drag_pressure_proxy": final.get("object_drag_pressure_proxy"),
        "pressure_delta": final.get("pressure_delta"),
        "throughput_delta": final.get("throughput_delta"),
        "outlet_throughput": final.get("outlet_throughput"),
        "vorticity_avg": final.get("vorticity_avg"),
        "vorticity_max": final.get("vorticity_max"),
        "render_frame": str(render_frame),
        "wind_projection_frame": str(projection_frame),
    }
    for key in (
        "object_projected_area",
        "outlet_throughput",
        "object_drag_pressure_proxy",
    ):
        if abs(float(metrics[key] or 0.0)) <= 1.0e-8:
            raise SystemExit(f"{case_dir.name}: metric {key} is zero/unavailable")
    return metrics


def write_reports(
    args: argparse.Namespace,
    results: list[dict[str, Any]],
    report_path: Path,
    json_path: Path,
) -> None:
    payload = {
        "schema": "physics_sim_wind_orientation_probe_v1",
        "source_runtime_scene": str(args.runtime_scene),
        "object_id": args.object_id or results[0].get("object_id"),
        "output_root": str(args.output_root),
        "grid": args.grid,
        "frames": args.frames,
        "sim_steps_per_frame": args.sim_steps_per_frame,
        "wind_visual_mode": args.wind_visual_mode,
        "wind_shot_camera": args.wind_shot_camera,
        "rotation_mode": args.rotation_mode,
        "object_candidates": discover_probe_objects(load_json(args.runtime_scene)),
        "results": results,
    }
    json_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    lines = [
        "PhysicsSim Wind orientation probe",
        f"source_runtime_scene: {args.runtime_scene}",
        f"object_id: {payload['object_id']}",
        f"output_root: {args.output_root}",
        f"grid: {args.grid}",
        f"frames: {args.frames}",
        f"sim_steps_per_frame: {args.sim_steps_per_frame}",
        f"wind_visual_mode: {args.wind_visual_mode}",
        f"rotation_mode: {args.rotation_mode}",
        "",
        (
            "orientation,rotation_deg,solid_cells,projected_area,"
            "object_pressure_delta,object_drag_pressure_proxy,pressure_delta,"
            "outlet_throughput,vorticity_avg,vorticity_max,render_frame,"
            "wind_projection_frame"
        ),
    ]
    for row in results:
        rotation = row["rotation_deg"]
        rotation_text = f"{rotation['x']}/{rotation['y']}/{rotation['z']}"
        lines.append(
            f"{row['orientation']},{rotation_text},{row['object_solid_cells']},"
            f"{row['object_projected_area']},{row['object_pressure_delta']},"
            f"{row['object_drag_pressure_proxy']},{row['pressure_delta']},"
            f"{row['outlet_throughput']},{row['vorticity_avg']},"
            f"{row['vorticity_max']},{row['render_frame']},"
            f"{row['wind_projection_frame']}"
        )
    lines.extend(["", f"json_summary: {json_path}"])
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-scene", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--object-id", default=None)
    parser.add_argument("--headless-bin", default="./physics_sim_headless", type=Path)
    parser.add_argument("--frames", default=12, type=int)
    parser.add_argument("--sim-steps-per-frame", default=2, type=int)
    parser.add_argument("--grid", default="96x24x24")
    parser.add_argument("--wind-visual-mode", default="volume_speed_deficit")
    parser.add_argument("--wind-shot-camera", default="side")
    parser.add_argument(
        "--rotation-mode",
        choices=("absolute", "relative"),
        default="absolute",
        help="Use absolute XYZ degree rotations or add them to the source rotation.",
    )
    parser.add_argument(
        "--orientation",
        action="append",
        default=[],
        type=parse_orientation,
        help="Orientation as name:x,y,z degree rotation. May be repeated.",
    )
    parser.add_argument(
        "--preset",
        choices=sorted(ORIENTATION_PRESETS),
        default="roll-sweep",
        help="Named orientation preset used when --orientation is omitted.",
    )
    parser.add_argument(
        "--list-objects",
        action="store_true",
        help="Print orientation-probe object candidates and exit.",
    )
    parser.add_argument(
        "--require-wind-tunnel",
        action="store_true",
        help="Fail unless the runtime scene has active extensions.physics_sim.wind_tunnel.",
    )
    parser.add_argument("--keep-existing", action="store_true")
    args = parser.parse_args(argv)
    if args.frames <= 0:
        parser.error("--frames must be positive")
    if args.sim_steps_per_frame <= 0:
        parser.error("--sim-steps-per-frame must be positive")
    if not args.runtime_scene.is_file():
        parser.error(f"--runtime-scene not found: {args.runtime_scene}")
    if not args.list_objects and not args.headless_bin.is_file():
        parser.error(f"--headless-bin not found: {args.headless_bin}")
    if not args.orientation:
        args.orientation = [
            parse_orientation(value) for value in ORIENTATION_PRESETS[args.preset]
        ]
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    source_scene = load_json(args.runtime_scene)
    if args.list_objects:
        print(format_object_listing(discover_probe_objects(source_scene)))
        return 0
    if args.require_wind_tunnel:
        require_wind_tunnel(source_scene, args.runtime_scene)
    if args.output_root.exists() and not args.keep_existing:
        shutil.rmtree(args.output_root)
    args.output_root.mkdir(parents=True, exist_ok=True)

    scene_dir = args.output_root / "runtime_scenes"
    results: list[dict[str, Any]] = []
    for orientation_name, rotation in args.orientation:
        case_name = safe_case_name(orientation_name)
        scene_path = scene_dir / f"{case_name}.json"
        object_id, applied_rotation = write_oriented_scene(
            source_scene,
            args.runtime_scene,
            scene_path,
            args.object_id,
            orientation_name,
            rotation,
            args.rotation_mode,
        )
        case_dir = args.output_root / case_name
        run_headless(args, scene_path, case_dir)
        metrics = read_final_metrics(case_dir, args.frames)
        row = {
            "orientation": orientation_name,
            "object_id": object_id,
            "runtime_scene": str(scene_path),
            "output_root": str(case_dir),
            "rotation_deg": applied_rotation,
        }
        row.update(metrics)
        results.append(row)

    drag_values = [float(row["object_drag_pressure_proxy"]) for row in results]
    area_values = [float(row["object_projected_area"]) for row in results]
    if len(results) > 1 and max(drag_values) - min(drag_values) <= 1.0e-8:
        raise SystemExit("orientation probe did not produce distinct drag-pressure proxies")
    if len(results) > 1 and max(area_values) - min(area_values) <= 1.0e-8:
        raise SystemExit("orientation probe did not produce distinct projected areas")

    report_path = args.output_root / "orientation_probe_summary.txt"
    json_path = args.output_root / "orientation_probe_summary.json"
    write_reports(args, results, report_path, json_path)
    print(report_path.read_text(encoding="utf-8"), end="")
    print(f"physics_sim Wind orientation probe passed: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
