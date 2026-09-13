#!/usr/bin/env python3
"""Validate PhysicsSim's independent worker-version source contract."""

from __future__ import annotations

import json
from pathlib import Path
import re
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
SEMVER_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")


class WorkerReleaseContractTests(unittest.TestCase):
    def test_source_contract_owns_an_independent_worker_version(self) -> None:
        app_version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
        worker_version = (ROOT / "WORKER_VERSION").read_text(encoding="utf-8").strip()
        contract = json.loads(
            (ROOT / "config/worker_release_contract.json").read_text(encoding="utf-8")
        )

        self.assertRegex(app_version, SEMVER_RE)
        self.assertRegex(worker_version, SEMVER_RE)
        self.assertEqual(contract["program"], "physics_sim")
        self.assertEqual(contract["worker_slug"], "physics_sim_headless_worker")
        self.assertEqual(contract["app_version_file"], "VERSION")
        self.assertEqual(contract["worker_version_file"], "WORKER_VERSION")
        self.assertEqual(contract["registry_target_key"], "worker_package")
        self.assertEqual(
            set(contract["package_platforms"]),
            {"linux-x86_64", "linux-aarch64"},
        )
        self.assertEqual(contract["package_targets_by_platform"], {
            "linux-aarch64": "package-linux-worker-self-test",
            "linux-x86_64": "package-linux-worker-x86_64-self-test",
        })

    def test_x86_target_is_a_distinct_platform_bound_entrypoint(self) -> None:
        completed = subprocess.run(
            ["make", "-n", "package-linux-worker-x86_64-self-test"],
            cwd=ROOT, check=False, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("LINUX_WORKER_PLATFORM=linux-x86_64", completed.stdout)

    def test_make_contract_keeps_app_and_worker_versions_distinct(self) -> None:
        completed = subprocess.run(
            [
                "make", "-s", "LINUX_WORKER_PLATFORM=linux-x86_64",
                "RELEASE_VERSION=1.2.3", "WORKER_VERSION=9.8.7",
                "package-linux-worker-contract",
            ],
            cwd=ROOT,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(
            completed.returncode, 0,
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}",
        )
        self.assertIn("worker version: 9.8.7", completed.stdout)
        self.assertIn("source program version: 1.2.3", completed.stdout)
        self.assertIn(
            "physics_sim-9.8.7-linux-x86_64-worker.tar.gz",
            completed.stdout,
        )
        self.assertNotIn("physics_sim-1.2.3-linux-x86_64-worker.tar.gz", completed.stdout)


if __name__ == "__main__":
    unittest.main()
