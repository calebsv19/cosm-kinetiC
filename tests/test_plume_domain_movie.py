"""Bounded tall movie transport, exact prefix parity and old-mode refusal."""
import copy
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
from test_open_atmosphere import request, WORKER
from open_atmosphere import validate
from plume_qualification import write_schedule, run_sparse, read_sample


class DomainMovieTests(unittest.TestCase):
    def config(self, nz=8, dt=.005):
        r = request(nz, dt)
        r['steps'] = []
        r['properties']['dynamic_viscosity_pa_s'] = .00002
        r['boundary_policy'].update(vertical='solid_bottom_open_top',
                                    predictor_velocity='no_slip_bottom_zero_gradient_top')
        r['resource_limits'] = {'max_cells': 524288,
                                'scalar_work_cells': 8000000000,
                                'numerical_bytes': 512 * 1024 * 1024,
                                'cache_pressure_operator': True}
        return r

    def test_tall_short_prefix_matches_existing_native_mode(self):
        r = self.config(128)
        old = copy.deepcopy(r)
        old['resource_limits']['scalar_work_cells'] = 1000000000
        cells = [[{'cell_index': 0, 'energy_transferred_j': .001,
                   'smoke_transferred_kg': 1e-8}]] * 2
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            totals = write_schedule(root / 'forcing', r, cells, [2], domain_movie=True)
            a = run_sparse(r, WORKER, root / 'forcing', root / 'new',
                           timeout_s=30, domain_movie=True)
            run_sparse(old, WORKER, root / 'forcing', root / 'old',
                       timeout_s=30, domain_qualification=True)
            self.assertEqual((root / 'new/sample-0002.bin').read_bytes(),
                             (root / 'old/sample-0002.bin').read_bytes())
            result = read_sample(root / 'new/sample-0002.bin', r,
                                 a['worker_sha256'], totals[-1], compact=True,
                                 domain_movie=True)
            self.assertEqual(result['fields']['time_s'], .01)
            self.assertAlmostEqual(result['budgets']['smoke_kg']['stored'], 2e-8)

    def test_forty_second_envelope_and_native_refusal(self):
        r = self.config(dt=.1)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            cells = [[{'cell_index': 0, 'energy_transferred_j': .01,
                       'smoke_transferred_kg': 1e-8}]] + [[]] * 399
            totals = write_schedule(root / 'forcing', r, cells,
                                    list(range(2, 401, 2)), domain_movie=True)
            receipt = run_sparse(r, WORKER, root / 'forcing', root / 'new',
                                 timeout_s=30, domain_movie=True)
            result = read_sample(root / 'new/sample-0400.bin', r,
                                 receipt['worker_sha256'], totals[-1], compact=True,
                                 domain_movie=True)
            self.assertEqual(receipt['time_s'], 40)
            self.assertEqual(len(list((root / 'new').glob('sample-*.bin'))), 200)
            self.assertAlmostEqual(result['budgets']['energy_j']['stored'], .01)
            self.assertAlmostEqual(result['budgets']['smoke_kg']['stored'], 1e-8)
            for mode in ({}, {'movie': True}, {'domain_qualification': True}):
                with self.assertRaises(ValueError):
                    validate(r, **mode)
            with self.assertRaises(ValueError):
                validate(r, movie=True, domain_movie=True)
            with self.assertRaises(ValueError):
                write_schedule(root / 'too-long', r, [[]] * 401, [401], domain_movie=True)
            blob = bytearray((root / 'forcing').read_bytes())
            struct.pack_into('<I', blob, 20, 8001)
            (root / 'bad').write_bytes(blob)
            with self.assertRaises(ValueError):
                run_sparse(r, WORKER, root / 'bad', root / 'rejected',
                           timeout_s=30, domain_movie=True)


if __name__ == '__main__':
    unittest.main()
