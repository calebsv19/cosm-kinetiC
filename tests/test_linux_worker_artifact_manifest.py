#!/usr/bin/env python3
"""Exercise the real Linux worker Make producer with local stub binaries."""

from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import tarfile
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class LinuxWorkerArtifactManifestTests(unittest.TestCase):
    def test_real_make_producer_emits_executor_artifact_set(self) -> None:
        with tempfile.TemporaryDirectory(prefix='worker-package-test-') as raw:
            temp = Path(raw).resolve()
            release = temp / "build/release"
            # Exercise the real fragment in a minimal isolated source checkout.
            # Recursive assembly must not inherit the application's compile graph.
            for directory in ('scripts','tools/packaging','config','docs','tests/fixtures/surface_sources'):
                shutil.copytree(ROOT/directory,temp/directory,ignore=shutil.ignore_patterns('__pycache__'))
            for name in ('README.md','VERSION','WORKER_VERSION'):
                shutil.copy2(ROOT/name,temp/name)
            headless = temp / "physics_sim_headless"
            runner = temp / "physics_sim_job_runner"
            headless.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            runner.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            headless.chmod(0o755)
            runner.chmod(0o755)
            wrapper = temp / "makefile"
            wrapper.write_text(textwrap.dedent(f"""\
                RELEASE_ROOT := {release}
                RELEASE_DIR := $(RELEASE_ROOT)
                RELEASE_PROGRAM_KEY := physics_sim
                RELEASE_VERSION := 0.3.2
                WORKER_VERSION := 0.3.3
                LINUX_WORKER_HOST_ARCH := arm64
                LINUX_WORKER_HOST_OS := Linux
                LINUX_WORKER_PLATFORM := linux-aarch64
                PHYSICS_SIM_HEADLESS_TOOL_BIN := {headless}
                PHYSICS_SIM_JOB_RUNNER_TOOL_BIN := {runner}
                SESSION_WORKER_BIN := {headless}
                PASSIVE3D_WORKER := {headless}
                ATMOSPHERE3D_WORKER := {headless}
                OPEN_ATMOSPHERE3D_WORKER := {headless}
                passive-atmosphere-worker:
                evolving-atmosphere-worker:
                open-atmosphere-worker:
                physics_sim_headless:
                physics-sim-job-runner:
                include {ROOT / 'make/package-linux-worker.mk'}
            """), encoding="utf-8")
            completed = subprocess.run(
                [
                    "make", "-f", str(wrapper),
                    f"RELEASE_ROOT={release}", f"RELEASE_DIR={release}",
                    "RELEASE_PROGRAM_KEY=physics_sim", "RELEASE_VERSION=0.3.2",
                    "WORKER_VERSION=0.3.3",
                    "LINUX_WORKER_HOST_ARCH=arm64", "LINUX_WORKER_HOST_OS=Linux",
                    "LINUX_WORKER_PLATFORM=linux-aarch64",
                    f"PHYSICS_SIM_HEADLESS_TOOL_BIN={headless}",
                    f"PHYSICS_SIM_JOB_RUNNER_TOOL_BIN={runner}",
                    f"SESSION_WORKER_BIN={headless}", f"PASSIVE3D_WORKER={headless}",
                    f"ATMOSPHERE3D_WORKER={headless}", f"OPEN_ATMOSPHERE3D_WORKER={headless}",
                    "package-linux-worker-self-test",
                ],
                cwd=temp, check=False, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, text=True,
            )
            self.assertEqual(
                completed.returncode, 0,
                f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}\n" + "\n".join(p.read_text(errors="replace")[-5000:] for p in release.rglob("*") if p.is_file() and ("stderr" in p.name or "stdout" in p.name)),
            )

            archive = release / "physics_sim-0.3.3-linux-aarch64-worker.tar.gz"
            checksum = archive.with_name(archive.name + ".sha256")
            manifest = archive.with_name(archive.name + ".manifest.txt")
            self.assertTrue(archive.is_file())
            self.assertTrue(checksum.is_file())
            self.assertTrue(manifest.is_file())
            stage=archive.with_name(archive.name.removesuffix('.tar.gz'))
            capabilities=subprocess.run([stage/'bin/physics_sim_coupling','capabilities'],cwd=temp,capture_output=True,text=True)
            self.assertEqual(capabilities.returncode,0,capabilities.stderr)
            self.assertEqual(set(json.loads(capabilities.stdout)['workers']),{'session','passive','evolving','open'})
            rogue=stage/'scripts/argparse.py';rogue.write_text('raise RuntimeError("must not import")')
            refused=subprocess.run([stage/'bin/physics_sim_coupling','capabilities'],cwd=temp,capture_output=True,text=True)
            self.assertNotEqual(refused.returncode,0)
            self.assertNotIn('must not import',refused.stderr)
            rogue.unlink()
            archive_digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            expected = (
                "program=physics_sim\n"
                "worker_slug=physics_sim_headless_worker\n"
                "version=0.3.3\n"
                "source_program_version=0.3.2\n"
                "platform=linux-aarch64\n"
                "package_role=headless-worker\n"
                "format=tar.gz\n"
                f"artifact={archive.name}\n"
                f"sha256={archive_digest}\n"
                "max_glibc_version=2.39.0\n"
            ).encode("utf-8")
            self.assertEqual(manifest.read_bytes(), expected)

            with tarfile.open(archive, "r:gz") as package:
                package_root = archive.name.removesuffix(".tar.gz")
                worker_manifest = json.load(
                    package.extractfile(f"{package_root}/manifest.json")
                )
                package_manifest = json.load(
                    package.extractfile(f"{package_root}/package_manifest.json")
                )
            for payload in (worker_manifest, package_manifest):
                self.assertEqual(payload["version"], "0.3.3")
                self.assertEqual(payload["source_program_version"], "0.3.2")

            first = manifest.read_bytes()
            subprocess.run([
                "python3", str(ROOT / "tools/packaging/write_linux_worker_artifact_manifest.py"),
                "--archive", str(archive), "--checksum", str(checksum),
                "--output", str(manifest), "--program", "physics_sim",
                "--version", "0.3.3", "--source-program-version", "0.3.2",
                "--platform", "linux-aarch64",
                "--worker-slug", "physics_sim_headless_worker",
                "--max-glibc-version", "2.39.0",
            ], check=True)
            self.assertEqual(manifest.read_bytes(), first)


if __name__ == "__main__":
    unittest.main()
