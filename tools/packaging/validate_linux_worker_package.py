#!/usr/bin/env python3
"""Validate the local PhysicsSim Linux worker package without remote execution."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile


EXPECTED_STAGE_FILES = (
    "README.md",
    "bin/physics_sim_headless",
    "bin/physics_sim_job_runner",
    "bin/run_worker.sh",
    "config/app.json",
    "docs/README.md",
    "docs/headless_cli.md",
    "manifest.json",
    "package_manifest.json",
)

FORBIDDEN_PACKAGE_PARTS = {
    "tmp",
    "_private_workspace_artifacts",
    "agent_runs",
}


def fail(message: str) -> None:
    print(f"worker package dry-run failed: {message}", file=sys.stderr)
    sys.exit(1)


def load_json(path: Path) -> dict:
    try:
        with path.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
    except OSError as exc:
        fail(f"could not read {path}: {exc}")
    except json.JSONDecodeError as exc:
        fail(f"invalid json in {path}: {exc}")
    if not isinstance(data, dict):
        fail(f"{path} must contain a json object")
    return data


def require_file(path: Path, executable: bool = False) -> None:
    if not path.is_file():
        fail(f"missing file: {path}")
    if executable and not os.access(path, os.X_OK):
        fail(f"file is not executable: {path}")


def validate_manifest(
    manifest: dict,
    program: str,
    version: str,
    platform: str,
    worker_slug: str,
) -> None:
    expected = {
        "schema_version": "codework-worker-package/v1",
        "worker_slug": worker_slug,
        "version": version,
        "platform": platform,
        "program": program,
        "entrypoint": "bin/run_worker.sh",
    }
    for key, value in expected.items():
        if manifest.get(key) != value:
            fail(f"manifest.json {key} expected {value!r}, got {manifest.get(key)!r}")
    if manifest.get("job_types") != ["trio_headless_stage"]:
        fail("manifest.json job_types must be ['trio_headless_stage']")
    if not isinstance(manifest.get("runtime_dependencies"), list):
        fail("manifest.json runtime_dependencies must be a list")


def validate_package_manifest(
    package_manifest: dict,
    program: str,
    version: str,
    platform: str,
    worker_slug: str,
) -> list[str]:
    expected = {
        "schema_version": "codework_worker_package_manifest_v1",
        "package_role": "headless-worker",
        "worker_slug": worker_slug,
        "program": program,
        "version": version,
        "platform": platform,
    }
    for key, value in expected.items():
        if package_manifest.get(key) != value:
            fail(
                f"package_manifest.json {key} expected {value!r}, "
                f"got {package_manifest.get(key)!r}"
            )
    entrypoints = package_manifest.get("entrypoints")
    if not isinstance(entrypoints, dict):
        fail("package_manifest.json entrypoints must be an object")
    if entrypoints.get("headless_cli") != "bin/physics_sim_headless":
        fail("package_manifest.json headless_cli entrypoint mismatch")
    if entrypoints.get("job_runner") != "bin/physics_sim_job_runner":
        fail("package_manifest.json job_runner entrypoint mismatch")
    self_test = package_manifest.get("self_test")
    if not isinstance(self_test, dict) or self_test.get("type") != "command":
        fail("package_manifest.json self_test must be a command object")
    argv = self_test.get("argv")
    if not isinstance(argv, list) or not argv:
        fail("package_manifest.json self_test.argv must be a non-empty list")
    if argv[0] != "bin/physics_sim_job_runner":
        fail("package_manifest.json self_test must exercise the job runner")
    return [str(arg) for arg in argv]


def validate_archive(archive: Path) -> None:
    if not archive.is_file():
        fail(f"missing archive: {archive}")
    try:
        with tarfile.open(archive, "r:gz") as tar:
            names = tar.getnames()
    except (OSError, tarfile.TarError) as exc:
        fail(f"could not read archive {archive}: {exc}")
    if not names:
        fail("archive is empty")

    top_roots = {Path(name).parts[0] for name in names if Path(name).parts}
    if len(top_roots) != 1:
        fail(f"archive must contain one package root, got {sorted(top_roots)}")
    package_root = next(iter(top_roots))
    archived = {str(Path(name)) for name in names}
    for relative in EXPECTED_STAGE_FILES:
        expected = str(Path(package_root) / relative)
        if expected not in archived:
            fail(f"archive missing {relative}")

    for name in names:
        parts = set(Path(name).parts)
        forbidden = parts & FORBIDDEN_PACKAGE_PARTS
        if forbidden:
            fail(f"archive contains forbidden generated/private lane {sorted(forbidden)}")


def run_manifest_self_test(stage_dir: Path, argv: list[str]) -> None:
    command = [str(stage_dir / argv[0]), *argv[1:]]
    try:
        result = subprocess.run(
            command,
            cwd=stage_dir,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except OSError as exc:
        fail(f"could not execute manifest self-test {command}: {exc}")
    if result.returncode != 0:
        fail(
            "manifest self-test command failed with "
            f"exit {result.returncode}: {' '.join(command)}\n{result.stderr.strip()}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate a local PhysicsSim Linux worker package dry-run."
    )
    parser.add_argument("--stage-dir", required=True)
    parser.add_argument("--archive", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--worker-slug", required=True)
    args = parser.parse_args()

    stage_dir = Path(args.stage_dir).resolve()
    archive = Path(args.archive).resolve()
    if not stage_dir.is_dir():
        fail(f"missing stage dir: {stage_dir}")

    for relative in EXPECTED_STAGE_FILES:
        require_file(stage_dir / relative, executable=relative.startswith("bin/"))

    run_worker = (stage_dir / "bin/run_worker.sh").read_text(encoding="utf-8")
    if "physics_sim_headless" not in run_worker or "exec" not in run_worker:
        fail("run_worker.sh must exec the staged physics_sim_headless binary")

    manifest = load_json(stage_dir / "manifest.json")
    package_manifest = load_json(stage_dir / "package_manifest.json")
    validate_manifest(manifest, args.program, args.version, args.platform, args.worker_slug)
    self_test_argv = validate_package_manifest(
        package_manifest,
        args.program,
        args.version,
        args.platform,
        args.worker_slug,
    )
    validate_archive(archive)
    run_manifest_self_test(stage_dir, self_test_argv)

    print(f"package-linux-worker-dry-run passed: {archive}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
