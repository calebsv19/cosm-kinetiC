"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_accuracy_admission_guard.py."""
import unittest
import test_cfd_reference3d_accuracy_admission_guard as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class AdmissionGuardArchive(unittest.TestCase):

    def stage(self, module, admitted):
        run = ast.parse(inspect.getsource(module.run)).body[0]
        sample = next((n for n in run.body if isinstance(n, ast.FunctionDef) and n.name == 'sample'))
        tree = ast.Module(body=[sample], type_ignores=[])
        admission = dict(numeric_stage_admitted=admitted, estimated_numeric_stage_bytes=9 * 2 ** 30 if not admitted else 5 * 2 ** 30, rss_cap_bytes=8192 * 2 ** 20, wall_cap_s=1800.0, basis_reservation_bytes=100, coarse_pressure_reservation_bytes=20)
        scope = dict(time=SimpleNamespace(monotonic=lambda: 10.0), resource=SimpleNamespace(RUSAGE_SELF=0, getrusage=lambda _: SimpleNamespace(ru_maxrss=100)), started=0.0, resource_samples={}, json=json, enforce_phase=lambda *a: None, coarse_pressure='quadratic', coarse_reserve=lambda *a: 20, nv=10, system=SimpleNamespace(volumes=[1.0, 1.0]), fresh_admission=lambda *a: admission.copy(), exact_factor=object(), basis_reservation=lambda *a: 100, C=SimpleNamespace(shape=(12, 12)), restart=6, PhaseResourceStopped=PhaseResourceStopped)
        exec(compile(tree, '<bounded admission stage>', 'exec'), scope)
        return scope['sample']

    def test_only_failure_record_and_module_routing_change_archive(self):
        for t in json.loads((archive_root() / 'c3d-accuracy-admission-guard/transforms.json').read_text()):
            v = (R / t['parent']).read_text()
            for a, b in t['literal_replacements']:
                self.assertIn(a, v)
                v = v.replace(a, b)
            self.assertEqual(v, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
