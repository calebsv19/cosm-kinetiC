#!/usr/bin/env python3
"""Validate the local PhysicsSim Linux worker package without remote execution."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import struct
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

ELF_MACHINE_BY_PLATFORM = {
    "linux-x86_64": 62,
    "linux-aarch64": 183,
}

PLATFORM_CAPABILITY_BY_PLATFORM = {
    "linux-x86_64": "platform-linux-x86_64-v1",
    "linux-aarch64": "platform-linux-aarch64-v1",
}

GLIBC_VERSION_RE = re.compile(r"\bGLIBC_([0-9]+(?:\.[0-9]+)+)\b")


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


def elf_machine(data: bytes) -> int | None:
    if len(data) < 20 or data[:4] != b"\x7fELF":
        return None
    if data[5] == 1:
        byte_order = "<"
    elif data[5] == 2:
        byte_order = ">"
    else:
        return None
    return int(struct.unpack(f"{byte_order}H", data[18:20])[0])


def validate_native_binary(path: Path, platform: str) -> None:
    expected_machine = ELF_MACHINE_BY_PLATFORM.get(platform)
    if expected_machine is None:
        fail(f"unsupported worker package platform: {platform}")
    try:
        data = path.read_bytes()[:64]
    except OSError as exc:
        fail(f"could not read native worker file {path}: {exc}")
    machine = elf_machine(data)
    if machine is None:
        fail(f"native worker file is not ELF: {path}")
    if machine != expected_machine:
        fail(
            f"native worker architecture mismatch for {path}: platform {platform} "
            f"requires ELF machine {expected_machine}, got {machine}"
        )


def version_tuple(value: str) -> tuple[int, ...]:
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+)+", value):
        fail(f"invalid GLIBC version: {value!r}")
    return tuple(int(part) for part in value.split("."))


def glibc_symbol_versions(path: Path) -> set[str]:
    try:
        result = subprocess.run(
            ["readelf", "--version-info", "--wide", str(path)],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except OSError as exc:
        fail(f"could not inspect GLIBC requirements for {path}: {exc}")
    if result.returncode != 0:
        fail(
            f"readelf failed for {path} with exit {result.returncode}: "
            f"{result.stderr.strip()}"
        )
    return set(GLIBC_VERSION_RE.findall(result.stdout))


def validate_glibc_ceiling(path: Path, maximum: str) -> str:
    allowed = version_tuple(maximum)
    versions = glibc_symbol_versions(path)
    if not versions:
        fail(f"native worker file has no readable GLIBC requirements: {path}")
    observed = max(versions, key=version_tuple)
    if version_tuple(observed) > allowed:
        fail(
            f"native worker GLIBC requirement exceeds fleet ceiling for {path}: "
            f"requires GLIBC_{observed}, maximum is GLIBC_{maximum}"
        )
    return observed


def validate_manifest(
    manifest: dict,
    program: str,
    version: str,
    platform: str,
    worker_slug: str,
    max_glibc_version: str,
) -> None:
    expected = {
        "schema_version": "codework-worker-package/v1",
        "worker_slug": worker_slug,
        "version": version,
        "platform": platform,
        "program": program,
        "entrypoint": "bin/run_worker.sh",
        "max_glibc_version": max_glibc_version,
    }
    for key, value in expected.items():
        if manifest.get(key) != value:
            fail(f"manifest.json {key} expected {value!r}, got {manifest.get(key)!r}")
    if manifest.get("job_types") != ["trio_headless_stage"]:
        fail("manifest.json job_types must be ['trio_headless_stage']")
    if not isinstance(manifest.get("runtime_dependencies"), list):
        fail("manifest.json runtime_dependencies must be a list")
    expected_capabilities = {
        "trio-headless-v1",
        "scene-project-portable-v1",
        "physics-cache-project-local-v1",
        PLATFORM_CAPABILITY_BY_PLATFORM[platform],
    }
    if set(manifest.get("capabilities") or []) != expected_capabilities:
        fail("manifest.json capabilities do not match the PhysicsSim package contract")


def validate_package_manifest(
    package_manifest: dict,
    program: str,
    version: str,
    platform: str,
    worker_slug: str,
    max_glibc_version: str,
) -> list[str]:
    expected = {
        "schema_version": "codework_worker_package_manifest_v1",
        "package_role": "headless-worker",
        "worker_slug": worker_slug,
        "program": program,
        "version": version,
        "platform": platform,
        "max_glibc_version": max_glibc_version,
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
    expected_capabilities = {
        "trio-headless-v1",
        "scene-project-portable-v1",
        "physics-cache-project-local-v1",
        PLATFORM_CAPABILITY_BY_PLATFORM[platform],
    }
    if set(package_manifest.get("capabilities") or []) != expected_capabilities:
        fail("package_manifest.json capabilities do not match the PhysicsSim package contract")
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
    parser.add_argument("--max-glibc-version", required=True)
    args = parser.parse_args()

    stage_dir = Path(args.stage_dir).resolve()
    archive = Path(args.archive).resolve()
    if not stage_dir.is_dir():
        fail(f"missing stage dir: {stage_dir}")

    for relative in EXPECTED_STAGE_FILES:
        require_file(stage_dir / relative, executable=relative.startswith("bin/"))

    validate_native_binary(stage_dir / "bin/physics_sim_headless", args.platform)
    validate_native_binary(stage_dir / "bin/physics_sim_job_runner", args.platform)
    headless_glibc = validate_glibc_ceiling(
        stage_dir / "bin/physics_sim_headless", args.max_glibc_version
    )
    runner_glibc = validate_glibc_ceiling(
        stage_dir / "bin/physics_sim_job_runner", args.max_glibc_version
    )

    run_worker = (stage_dir / "bin/run_worker.sh").read_text(encoding="utf-8")
    if "physics_sim_headless" not in run_worker or "exec" not in run_worker:
        fail("run_worker.sh must exec the staged physics_sim_headless binary")

    manifest = load_json(stage_dir / "manifest.json")
    package_manifest = load_json(stage_dir / "package_manifest.json")
    validate_manifest(
        manifest,
        args.program,
        args.version,
        args.platform,
        args.worker_slug,
        args.max_glibc_version,
    )
    self_test_argv = validate_package_manifest(
        package_manifest,
        args.program,
        args.version,
        args.platform,
        args.worker_slug,
        args.max_glibc_version,
    )
    validate_archive(archive)
    run_manifest_self_test(stage_dir, self_test_argv)

    print(
        f"package-linux-worker-dry-run passed: {archive} "
        f"(GLIBC ceiling {args.max_glibc_version}; observed "
        f"headless {headless_glibc}, job runner {runner_glibc})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
