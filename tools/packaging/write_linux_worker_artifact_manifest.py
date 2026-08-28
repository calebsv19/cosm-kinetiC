#!/usr/bin/env python3
"""Write or verify the deterministic Linux worker artifact sidecar manifest."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import sys


SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def checksum_digest(path: Path) -> str:
    fields = path.read_text(encoding="utf-8").strip().split()
    if not fields or not SHA256_RE.fullmatch(fields[0]):
        raise ValueError("worker checksum sidecar does not start with SHA-256")
    return fields[0]


def manifest_bytes(
    *, archive: Path, checksum: Path, program: str, version: str,
    platform: str, worker_slug: str, max_glibc_version: str,
) -> bytes:
    archive_digest = sha256(archive)
    if checksum_digest(checksum) != archive_digest:
        raise ValueError("worker checksum sidecar does not match archive bytes")
    fields = (
        ("program", program),
        ("worker_slug", worker_slug),
        ("version", version),
        ("platform", platform),
        ("package_role", "headless-worker"),
        ("format", "tar.gz"),
        ("artifact", archive.name),
        ("sha256", archive_digest),
        ("max_glibc_version", max_glibc_version),
    )
    for key, value in fields:
        if not value or "\n" in value or "\r" in value:
            raise ValueError(f"worker artifact manifest {key} is invalid")
    return "".join(f"{key}={value}\n" for key, value in fields).encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", required=True)
    parser.add_argument("--checksum", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--worker-slug", required=True)
    parser.add_argument("--max-glibc-version", required=True)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()

    archive = Path(args.archive)
    checksum = Path(args.checksum)
    output = Path(args.output)
    expected = manifest_bytes(
        archive=archive, checksum=checksum, program=args.program,
        version=args.version, platform=args.platform,
        worker_slug=args.worker_slug,
        max_glibc_version=args.max_glibc_version,
    )
    if args.verify:
        if not output.is_file() or output.read_bytes() != expected:
            raise ValueError("worker artifact sidecar manifest drifted")
        return 0
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(expected)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"worker artifact manifest failed: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
