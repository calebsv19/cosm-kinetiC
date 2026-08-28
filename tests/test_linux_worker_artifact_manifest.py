#!/usr/bin/env python3
"""Exercise the real Linux worker Make producer with local stub binaries."""

from __future__ import annotations

import hashlib
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class LinuxWorkerArtifactManifestTests(unittest.TestCase):
    def test_real_make_producer_emits_executor_artifact_set(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            temp = Path(raw)
            release = temp / "release"
            headless = temp / "physics_sim_headless"
            runner = temp / "physics_sim_job_runner"
            headless.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            runner.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            headless.chmod(0o755)
            runner.chmod(0o755)
            wrapper = temp / "producer.mk"
            wrapper.write_text(textwrap.dedent(f"""\
                RELEASE_ROOT := {release}
                RELEASE_DIR := $(RELEASE_ROOT)
                RELEASE_PROGRAM_KEY := physics_sim
                RELEASE_VERSION := 0.3.2
                LINUX_WORKER_HOST_ARCH := arm64
                LINUX_WORKER_HOST_OS := Linux
                LINUX_WORKER_PLATFORM := linux-aarch64
                PHYSICS_SIM_HEADLESS_TOOL_BIN := {headless}
                PHYSICS_SIM_JOB_RUNNER_TOOL_BIN := {runner}
                physics_sim_headless:
                physics-sim-job-runner:
                include {ROOT / 'make/package-linux-worker.mk'}
            """), encoding="utf-8")
            completed = subprocess.run(
                [
                    "make", "-f", str(wrapper),
                    f"RELEASE_ROOT={release}", f"RELEASE_DIR={release}",
                    "RELEASE_PROGRAM_KEY=physics_sim", "RELEASE_VERSION=0.3.2",
                    "LINUX_WORKER_HOST_ARCH=arm64", "LINUX_WORKER_HOST_OS=Linux",
                    "LINUX_WORKER_PLATFORM=linux-aarch64",
                    f"PHYSICS_SIM_HEADLESS_TOOL_BIN={headless}",
                    f"PHYSICS_SIM_JOB_RUNNER_TOOL_BIN={runner}",
                    "package-linux-worker-self-test",
                ],
                cwd=ROOT, check=False, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, text=True,
            )
            self.assertEqual(
                completed.returncode, 0,
                f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}",
            )

            archive = release / "physics_sim-0.3.2-linux-aarch64-worker.tar.gz"
            checksum = archive.with_name(archive.name + ".sha256")
            manifest = archive.with_name(archive.name + ".manifest.txt")
            self.assertTrue(archive.is_file())
            self.assertTrue(checksum.is_file())
            self.assertTrue(manifest.is_file())
            archive_digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            expected = (
                "program=physics_sim\n"
                "worker_slug=physics_sim_headless_worker\n"
                "version=0.3.2\n"
                "platform=linux-aarch64\n"
                "package_role=headless-worker\n"
                "format=tar.gz\n"
                f"artifact={archive.name}\n"
                f"sha256={archive_digest}\n"
                "max_glibc_version=2.39.0\n"
            ).encode("utf-8")
            self.assertEqual(manifest.read_bytes(), expected)

            first = manifest.read_bytes()
            subprocess.run([
                "python3", str(ROOT / "tools/packaging/write_linux_worker_artifact_manifest.py"),
                "--archive", str(archive), "--checksum", str(checksum),
                "--output", str(manifest), "--program", "physics_sim",
                "--version", "0.3.2", "--platform", "linux-aarch64",
                "--worker-slug", "physics_sim_headless_worker",
                "--max-glibc-version", "2.39.0",
            ], check=True)
            self.assertEqual(manifest.read_bytes(), first)


if __name__ == "__main__":
    unittest.main()
