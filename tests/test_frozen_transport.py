"""Explicit frozen-mode admission; ordinary channel defaults remain strict."""
import copy
import json
import os
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import open_atmosphere as native
from coupling.frozen_transport import validate, MODE


class FrozenTransportTests(unittest.TestCase):
    def setUp(self):
        self.f = json.loads((Path(os.environ['REPLAY_DONOR']) / 'config.json').read_text())['fluid']
        self.f['buoyancy']['enabled'] = False

    def request(self):
        return dict(self.f, schema=native.SCHEMA, state=None, steps=[])

    def test_ordinary_channel_still_refuses_nonzero_initial_field(self):
        self.f['initial_face_velocity_m_s'][0] = .1
        with self.assertRaisesRegex(ValueError, 'still gas'):
            native.validate(self.request())

    def test_explicit_mode_and_configuration_binding(self):
        original = native.configuration(self.f)
        self.f['diagnostic_transport'] = MODE
        native.validate(self.request())
        self.assertNotEqual(native.configuration(self.f), original)
        self.assertEqual(native.configuration(self.f)['diagnostic_transport'], MODE)

    def test_unknown_mode_and_buoyancy_refused(self):
        self.f['diagnostic_transport'] = 'unknown'
        with self.assertRaises(ValueError):native.validate(self.request())
        self.f['diagnostic_transport'] = MODE
        self.f['buoyancy']['enabled'] = True
        with self.assertRaises(ValueError):native.validate(self.request())

    def test_variable_wind_refused(self):
        self.f['diagnostic_transport'] = MODE
        self.f['wind_forcing']['knots'].append(dict(step=20,speed_m_s=0))
        with self.assertRaises(ValueError):native.validate(self.request())

    def test_velocity_mutation_refused(self):
        self.f['diagnostic_transport'] = MODE
        data = {'face_velocity_m_s': copy.deepcopy(self.f['initial_face_velocity_m_s'])}
        validate(self.f, data)
        data['face_velocity_m_s'][0] = .1
        with self.assertRaisesRegex(ValueError, 'field changed'):validate(self.f, data)


if __name__ == '__main__':
    unittest.main()
