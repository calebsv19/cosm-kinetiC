#!/usr/bin/env python3
import importlib.util
import tempfile
import struct
import unittest
from unittest import mock
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "packaging" / "validate_linux_worker_package.py"
SPEC = importlib.util.spec_from_file_location("validate_linux_worker_package", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def fake_elf(machine: int, byte_order: str = "<") -> bytes:
    data = bytearray(64)
    data[:4] = b"\x7fELF"
    data[4] = 2
    data[5] = 1 if byte_order == "<" else 2
    data[18:20] = struct.pack(f"{byte_order}H", machine)
    return bytes(data)


class LinuxWorkerPackageArchitectureTests(unittest.TestCase):
    def test_reads_x86_64_elf_machine(self) -> None:
        self.assertEqual(MODULE.elf_machine(fake_elf(62)), 62)

    def test_reads_aarch64_elf_machine(self) -> None:
        self.assertEqual(MODULE.elf_machine(fake_elf(183, ">")), 183)

    def test_rejects_non_elf_bytes(self) -> None:
        self.assertIsNone(MODULE.elf_machine(b"Mach-O"))

    def test_validator_rejects_wrong_architecture(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "worker"
            binary.write_bytes(fake_elf(183))
            with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
                MODULE.validate_native_binary(binary, "linux-x86_64")

    def test_validator_rejects_mach_o(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "worker"
            binary.write_bytes(b"Mach-O arm64")
            with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
                MODULE.validate_native_binary(binary, "linux-x86_64")

    def test_reads_maximum_glibc_symbol_requirement(self) -> None:
        completed = mock.Mock(
            returncode=0,
            stdout="Name: GLIBC_2.17  Flags: none\nName: GLIBC_2.39  Flags: none\n",
            stderr="",
        )
        with mock.patch.object(MODULE.subprocess, "run", return_value=completed):
            self.assertEqual(
                MODULE.validate_glibc_ceiling(Path("worker"), "2.39.0"),
                "2.39",
            )

    def test_rejects_glibc_requirement_above_fleet_ceiling(self) -> None:
        completed = mock.Mock(
            returncode=0,
            stdout="Name: GLIBC_2.17  Flags: none\nName: GLIBC_2.43  Flags: none\n",
            stderr="",
        )
        with mock.patch.object(MODULE.subprocess, "run", return_value=completed):
            with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
                MODULE.validate_glibc_ceiling(Path("worker"), "2.39.0")


if __name__ == "__main__":
    unittest.main()
