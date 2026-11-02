"""Physical invariants for the three-grid wake and Fire surface controls."""
import copy
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.wake_reference import gas_grid, fire_surface
from coupling.wind_presets import configuration
from coupling.wind_sensitivity import refined
from open_atmosphere import validate, SCHEMA


class WakeReference(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        workers = tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT',
            'FIRE_FLUID_FIRE_WORKER', 'FIRE_FLUID_OPEN_WORKER', 'FIRE_FLUID_CLOCK_WORKER'))
        cls.base = refined(configuration(workers, 'abrupt_gust'), 64)
        f = cls.base['fluid']; f['momentum_dt_s'] = .0025
        cls.base['periods'][1] = [1, 400]
        for knot in f['wind_forcing']['knots']: knot['step'] *= 2
        f['wind_forcing']['momentum_backflow'] = 'quiescent_exterior/v1'
        f['solid_box'] = {'schema':'physics_sim_stationary_box/v1',
            'lower_m':[.375]*3,'upper_m':[.625]*3,
            'velocity_boundary':'no_slip','thermal_boundary':'insulated'}
        f['resource_limits'] = {'max_cells':64**3,'scalar_work_cells':1000000000,
            'numerical_bytes':512*1024**2,'cache_pressure_operator':True,
            'native_request_bytes':256*1024**2,'native_output_bytes':256*1024**2}

    def test_gas_series_invariants_and_native_admission(self):
        for n in (48, 64, 80):
            c = gas_grid(self.base, n)
            validate(dict(c['fluid'], schema=SCHEMA, state=None, steps=[]))
            self.assertEqual(c['source'], self.base['source'])
            for k in ('wind_forcing', 'solid_box', 'properties', 'momentum_dt_s'):
                self.assertEqual(c['fluid'][k], self.base['fluid'][k])
            self.assertEqual(c['periods'], self.base['periods'])
            self.assertEqual(len(c['receiver']['injection_fluid_mask']), n**3//8)
            self.assertEqual(n*.25, {48:12, 64:16, 80:20}[n])

    def test_fire_nested_nodes_same_physics(self):
        c = fire_surface(self.base)
        self.assertEqual(c['fluid'], self.base['fluid'])
        self.assertEqual(c['receiver'], self.base['receiver'])
        self.assertEqual(c['source']['evolution'], self.base['source']['evolution'])
        self.assertEqual(c['source']['calibration'], self.base['source']['calibration'])
        for i in range(17):
            self.assertEqual(i*self.base['source']['cell_size_m'], 2*i*c['source']['cell_size_m'])
        self.assertEqual(c['periods'], self.base['periods'])

    def test_unsupported_or_changed_controls_refused(self):
        for n in (True, 32, 96):
            with self.assertRaises(ValueError): gas_grid(self.base, n)
        for key,value in (('momentum_dt_s', .005), ('length_m', [2,1,1])):
            c = copy.deepcopy(self.base); c['fluid'][key] = value
            with self.assertRaises(ValueError): gas_grid(c, 48)
        c = copy.deepcopy(self.base); c['source']['width'] = 16
        with self.assertRaises(ValueError): fire_surface(c)

    def test_no_input_mutation(self):
        original = copy.deepcopy(self.base)
        gas_grid(self.base, 48); fire_surface(self.base)
        self.assertEqual(self.base, original)


if __name__ == '__main__': unittest.main()
