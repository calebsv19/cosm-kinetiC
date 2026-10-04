"""Reject insufficient/contradictory physical qualification logs."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qualification', ROOT/'scripts/verify_cfd_refined_local_accuracy.py')
qualification = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qualification)

# Controlled records exercise evidence admission, not numerical accuracy.
VALID = '''domain_length_m=4 base_dx_m=0.03125
local_levels=7 lattice_scale=128
energy_strain=1 energy_boundary=1
n=64 uniform=0 cells=18240 pressure=0.003633153361585 viscous=0.002347529940743 total=0.005980683302328 divergence=0 closed_balance=0 lift=0
fixed_stokes_obstacle_physical_gate=passed relative_linear_residual=1e-12
'''


class EvidenceTests(unittest.TestCase):
    def read(self, text):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root)/'case.log'
            path.write_text(text)
            return qualification.read(path)

    def test_accepts_explicit_reference_and_residual(self):
        self.assertTrue(self.read(VALID)['physically_qualified'])

    def test_rejects_wrong_missing_or_failed_evidence(self):
        cases = [VALID.replace('domain_length_m=4', 'domain_length_m=6'),
                 VALID.replace('base_dx_m=0.03125', 'base_dx_m=0.0625'),
                 VALID.replace('relative_linear_residual=1e-12', 'relative_linear_residual=1e-3'),
                 VALID.replace('relative_linear_residual=1e-12', 'relative_linear_residual=nan'),
                 '\n'.join(VALID.splitlines()[1:]),
                 '\n'.join(VALID.splitlines()[:-1])]
        for text in cases:
            with self.subTest(text=text):
                with self.assertRaises((AssertionError, StopIteration)):
                    self.read(text)


if __name__ == '__main__': unittest.main()
