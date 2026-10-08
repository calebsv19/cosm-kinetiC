"""Bounded analytic momentum, thermal-time and pressure/force qualification."""
import argparse
import json
import math
from pathlib import Path
import unittest
from test_open_atmosphere import request, run, WORKER

METRICS = {}

class ConvergenceTests(unittest.TestCase):
    def test_advected_viscous_shear_spatial_refinement(self):
        # Exact incompressible solution: ux=A exp(-nu k^2 t) cos(k(y-Ut)),
        # uy=U, uz=0, p=0. Open Z closures are satisfied identically.
        rows = []
        for size in (8, 16, 32):
            r = request(4, .0025)
            r['grid'] = [size, size, 4]
            n, plane = size * size * 4, size * size
            zero = [0.] * n
            r['initial_energy_j'] = zero.copy()
            r['initial_smoke_kg'] = zero.copy()
            r['initial_face_velocity_m_s'] = [
                .2 * math.cos(math.pi * (q // size % size + .5) * 2 / size)
                for q in range(n)] + [.3] * n + [0.] * (n + plane)
            r['steps'] = [{'energy_j': zero, 'smoke_kg': zero}] * 40
            f = run(r, WORKER)['fields']
            exact = [.2 * math.exp(-.02 * math.pi ** 2 * .1) *
                     math.cos(math.pi * ((q // size % size + .5) * 2 / size - .3 * .1))
                     for q in range(n)]
            error = math.sqrt(math.fsum((a-b)**2 for a,b in
                zip(exact, f['face_velocity_m_s'][:n])) / n)
            self.assertLess(f['max_divergence_s_inv'], 1e-10)
            self.assertLess(max(abs(v-.3) for v in f['face_velocity_m_s'][n:2*n]), 1e-12)
            self.assertLess(max(abs(v) for v in f['face_velocity_m_s'][2*n:]), 1e-12)
            rows.append({'horizontal_cells': size, 'dt_s': .0025,
                         'velocity_rms_error_m_s': error})
        ratios = [rows[i+1]['velocity_rms_error_m_s']/rows[i]['velocity_rms_error_m_s']
                  for i in range(2)]
        for ratio in ratios:
            self.assertGreater(ratio, .4)
            self.assertLess(ratio, .65)
        METRICS['advected_viscous_shear'] = {'samples': rows, 'error_ratios': ratios}

    def test_source_driven_thermal_time_refinement(self):
        # Uniform heating at 3 K/s gives continuous v=g beta (3)t^2/2.
        # Small declared gravity isolates temporal lag: boundary cooling is
        # independently required to perturb temperature by less than 1e-8 K.
        rows = []
        exact = .001 / 300 * 3 * .1 ** 2 / 2
        for dt in (.02, .01, .005):
            r = request(8, dt)
            n, capacity = 512, 1000 * .25 ** 3
            r['buoyancy']['enabled'] = True
            r['buoyancy']['gravity_m_s2'] = .001
            r['steps'] = [{'energy_j': [capacity * 3 * dt] * n,
                           'smoke_kg': [0.] * n}] * round(.1/dt)
            result = run(r, WORKER)
            f = result['fields']
            temperatures = [e / capacity for e in f['energy_j']]
            self.assertLess(max(abs(t-.3) for t in temperatures), 1e-8)
            velocity = math.fsum(f['face_velocity_m_s'][2*n:]) / (n+64)
            error = abs(velocity-exact)
            self.assertGreater(velocity, 0)
            self.assertLess(f['max_divergence_s_inv'], 1e-10)
            rows.append({'dt_s': dt, 'velocity_m_s': velocity,
                         'continuous_velocity_m_s': exact, 'absolute_error_m_s': error})
        ratios = [rows[i+1]['absolute_error_m_s']/rows[i]['absolute_error_m_s']
                  for i in range(2)]
        for ratio in ratios:
            self.assertAlmostEqual(ratio, .5, delta=1e-6)
        METRICS['source_driven_thermal_time'] = {'samples': rows, 'error_ratios': ratios,
            'gravity_m_s2': .001, 'heating_k_s': 3,
            'qualification': 'temporal lag control with negligible boundary cooling; not plume convergence'}

    def test_independent_pressure_and_thermal_force_superposition(self):
        rows = []
        thermal = 9.81 * 3 / 300
        for nz, dt in ((8, .02), (16, .01)):
            for pressure_acceleration in (-thermal, 0., thermal):
                r = request(nz, dt)
                n, capacity = 64*nz, 1000*.25**2*(2/nz)
                r['initial_energy_j'] = [3*capacity]*n
                r['boundary_policy']['inflow_temperature_k'] = [303, 303]
                r['boundary_policy']['pressure_datum_pa'] = [pressure_acceleration, -pressure_acceleration]
                r['buoyancy']['enabled'] = True
                r['steps'] = r['steps'][:round(.1/dt)]
                f = run(r, WORKER)['fields']
                expected = (thermal+pressure_acceleration)*.1
                error = max(abs(v-expected) for v in f['face_velocity_m_s'][2*n:])
                pressure_exact = [-pressure_acceleration*((q//64+.5)*2/nz-1) for q in range(n)]
                p_error = max(abs(a-b) for a,b in zip(pressure_exact,f['pressure_pa']))
                self.assertLess(error, 1e-11)
                self.assertLess(p_error, 1e-10)
                rows.append({'nz': nz, 'dt_s': dt, 'pressure_acceleration_m_s2': pressure_acceleration,
                             'expected_velocity_m_s': expected, 'max_velocity_error_m_s': error,
                             'max_pressure_error_pa': p_error})
        METRICS['pressure_thermal_superposition'] = rows

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.report:
        from cfd_evidence import admitted_path,experiment_root
        from test_open_atmosphere import ROOT
        args.report=admitted_path(args.report)
        experiment_root(ROOT,args.report.parent)
        if args.report.exists():parser.error('Report output exists; select a fresh retained path')
    result = unittest.TextTestRunner(verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(ConvergenceTests))
    if result.wasSuccessful() and args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        with args.report.open('x') as stream:stream.write(json.dumps({'schema': 'physics_sim_open_convergence/v1',
            'tests_passed': result.testsRun, 'metrics': METRICS}, indent=2)+'\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)
