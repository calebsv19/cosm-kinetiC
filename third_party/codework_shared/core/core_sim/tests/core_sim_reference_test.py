#!/usr/bin/env python3
"""Process-level proof of the example host; standard library only."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

BINARY = Path(sys.argv[1]).resolve()
REPORT = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
RUNS = []
MASK = (1 << 64) - 1
FNV = 14695981039346656037


def mix(h, value):
    for byte in value.to_bytes(8, "little"):
        h = ((h ^ byte) * 1099511628211) & MASK
    return h


def expected_mass(mode, ticks):
    # Independent piecewise solution: first window has no return signal;
    # a unit return signal doubles subsequent rate only in nonzero mode.
    return ticks if mode != 2 or ticks <= 120 else 120 + 2 * (ticks - 120)


def expected_trajectory(mode, windows):
    h = FNV
    for w in range(1, windows + 1):
        amount = expected_mass(mode, w * 120)
        for v in (w, amount, amount, 10 * w, 40 * w, 1):
            h = mix(h, v)
    return h


def expected_observations(mode, fps, windows):
    h = FNV
    for t in range(0, windows * 120 + 1, 600 // fps):
        amount = expected_mass(mode, t)
        for v in (t, amount, 1, amount, 1):
            h = mix(h, v)
    return h


def invoke(path, mode=2, fps=25, resume=False, stop=200, fault=None, expected=0):
    args = [str(BINARY), "--checkpoint", str(path), "--mode", str(mode),
            "--fps", str(fps), "--stop-after", str(stop)]
    if resume:
        args.append("--resume")
    if fault is not None:
        args += ["--fail-window", str(fault[0]), "--fail-phase", str(fault[1])]
    start = time.perf_counter_ns()
    result = subprocess.run(args, capture_output=True, text=True, timeout=30)
    if result.returncode != expected:
        raise AssertionError((args, result.returncode, result.stdout, result.stderr))
    summary = json.loads(result.stdout) if result.stdout else None
    RUNS.append({"mode": mode, "fps": fps, "resume": resume, "stop": stop,
                 "fault": fault, "exit": result.returncode,
                 "host_elapsed_wall_ns": time.perf_counter_ns() - start, "result": summary})
    return summary


def numerical(result):
    return {k: result[k] for k in ("windows", "model_ticks", "ticks_per_second", "emitted",
                                   "received", "source_steps", "receiver_steps", "signal", "trajectory")}


def rechecksum(text):
    body = text.split("CHECKSUM")[0]
    h = FNV
    for byte in body.encode("ascii"):
        h = ((h ^ byte) * 1099511628211) & MASK
    return body + f"CHECKSUM {h:016x}\n"


class ReferenceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="t5-reference-", dir=BINARY.parent)
        cls.root = Path(cls.directory.name)
        cls.baseline_path = cls.root / "baseline.checkpoint"
        cls.baseline = invoke(cls.baseline_path)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def test_full_matrix(self):
        for mode in (0, 1, 2):
            reference = None
            for fps in (5, 25, 60):
                with self.subTest(mode=mode, fps=fps):
                    result = invoke(self.root / f"matrix-{mode}-{fps}", mode, fps)
                    self.assertEqual(result["windows"], 200)
                    self.assertEqual(result["model_ticks"], 24000)
                    self.assertEqual(result["ticks_per_second"], 600)
                    self.assertEqual(result["source_steps"], 2000)
                    self.assertEqual(result["receiver_steps"], 8000)
                    self.assertEqual(result["emitted"], expected_mass(mode, 24000))
                    self.assertEqual(result["received"], result["emitted"])
                    self.assertEqual(result["trajectory"], expected_trajectory(mode, 200))
                    self.assertEqual(result["observations"], 40 * fps + 1)
                    self.assertEqual(result["observation_hash"], expected_observations(mode, fps, 200))
                    if reference is None:
                        reference = numerical(result)
                    self.assertEqual(numerical(result), reference)
                    if fps == 60:
                        self.assertGreater(result["fractional_brackets"], 0)
                    self.assertGreaterEqual(result["elapsed_wall_ns"], 0)
        self.assertEqual((self.root / "matrix-0-25").read_text().splitlines()[1],
                         (self.root / "matrix-1-25").read_text().splitlines()[1])
        self.assertEqual(numerical(invoke(self.root / "zero-control", mode=1)),
                         numerical(invoke(self.root / "disabled-control", mode=0)))

    def test_known_prefixes(self):
        for w, amount in ((1, 120), (2, 360), (3, 600)):
            result = invoke(self.root / f"prefix-{w}", stop=w)
            self.assertEqual(result["emitted"], amount)
            self.assertEqual(result["received"], amount)
            self.assertEqual(result["source_steps"], 10*w)
            self.assertEqual(result["receiver_steps"], 40*w)

    def test_failures_and_uncertain_publication(self):
        for window, phase in ((0, 1), (7, 1), (7, 2), (7, 3), (199, 3)):
            with self.subTest(window=window, phase=phase):
                path = self.root / f"fault-{window}-{phase}"
                invoke(path, fault=(window, phase), expected=75)
                committed = window + (phase == 3)
                control = self.root / f"control-{window}-{phase}"
                invoke(control, stop=committed)
                self.assertEqual(path.read_bytes(), control.read_bytes())
                result = invoke(path, fps=60, resume=True)
                self.assertEqual(numerical(result), numerical(self.baseline))
                self.assertEqual(path.read_bytes(), self.baseline_path.read_bytes())
                # Completed restart executes no native steps or accepted transfers.
                original = path.read_bytes()
                again = invoke(path, resume=True)
                self.assertEqual(numerical(again), numerical(self.baseline))
                self.assertEqual(path.read_bytes(), original)
                self.assertEqual(again["observations"], 0)

    def test_resumed_vs_uninterrupted(self):
        for mode in (0, 1, 2):
            path = self.root / f"resume-{mode}"
            invoke(path, mode=mode, fps=5, stop=73)
            result = invoke(path, mode=mode, fps=60, resume=True)
            self.assertEqual(result["trajectory"], expected_trajectory(mode, 200))
            self.assertEqual(result["emitted"], expected_mass(mode, 24000))
            self.assertEqual(path.read_bytes(), self.whole(mode))

    def whole(self, mode):
        path = self.root / f"resume-whole-{mode}"
        invoke(path, mode=mode)
        return path.read_bytes()

    def test_checkpoint_refusals(self):
        path = self.root / "bad"
        invoke(path, stop=7)
        original = path.read_bytes()
        invoke(path, expected=1)  # Fresh run never replaces an existing state.
        self.assertEqual(path.read_bytes(), original)
        invoke(path, mode=1, resume=True, expected=1)
        self.assertEqual(path.read_bytes(), original)
        text = original.decode("ascii")
        damaged = [text[:-12], text + "extra\n", text.replace("T5REF1", "T5REF2"),
                   text.replace("CHECKSUM ", "CHECKSUM f")]
        lines = text.splitlines(keepends=True)
        tokens = lines[1].split(); tokens[1] = str(int(tokens[1]) + 1)
        damaged.append(rechecksum(lines[0] + " ".join(tokens) + "\n" + lines[2]))
        tokens = lines[0].split(); tokens[4] = "999"
        damaged.append(rechecksum(" ".join(tokens) + "\n" + lines[1] + lines[2]))
        for content in damaged:
            path.write_text(content, encoding="ascii")
            invoke(path, resume=True, expected=1)
            self.assertEqual(path.read_text(), content)
        self.assertFalse(list(self.root.glob("*.tmp.*")))


if __name__ == "__main__":
    started = time.perf_counter_ns()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ReferenceTest)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {"schema": "core_sim_t5_process_qualification/v1", "tests_run": result.testsRun,
              "failures": len(result.failures), "errors": len(result.errors),
              "elapsed_wall_ns": time.perf_counter_ns()-started, "runs": RUNS}
    if REPORT:
        REPORT.write_text(json.dumps(report, indent=2) + "\n")
    raise SystemExit(not result.wasSuccessful())
