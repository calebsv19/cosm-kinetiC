"""Retained-pulse provenance, physical mapping and clock partition controls."""
import copy
from fractions import Fraction
import json
import math
import os
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from coupling.source_replay import Replay, extract, validate
from coupling.overnight_cases import configuration
from coupling.allocation import allocate_window


class SourceReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pulse = extract(Path(os.environ['REPLAY_DONOR']))
        cls.workers = tuple(json.loads(Path(os.environ['REPLAY_WORKERS']).read_text())[k]
                            for k in ('growth_root',))
        w = json.loads(Path(os.environ['REPLAY_WORKERS']).read_text())
        cls.workers += tuple(w['binaries'][k]['path'] for k in ('fire', 'fluid', 'clock'))

    def config(self, n):
        return configuration(self.workers, 'spatial64' if n == 64 else
                             'exchange32' if n == 32 else 'gust16')

    def test_original_pulse_total_and_clock(self):
        self.assertEqual(self.pulse['end_s'], 1.6)
        self.assertEqual(len(self.pulse['frames']), 8)
        self.assertAlmostEqual(self.pulse['totals']['energy_transferred_j'], 1.2062387959304033)
        self.assertEqual(self.pulse['feedback'], 'recorded_donor_only_no_live_return')

    def test_original_window_allocation_is_exact(self):
        c = self.config(16)
        frame = self.pulse['frames'][0]
        policy = dict(schema='physics_sim_surface_source_policy/v1',
                      consumer_id='retained-pulse-replay', model_id='fixed-physical-pulse',
                      source_config=frame['config'], producer=frame['producer'],
                      stream_start_tick=0, receiver=c['receiver'])
        window = {'steps': [dict(left_n=i, left_d=40, right_n=i+1, right_d=40)
                            for i in range(40)]}
        expected = allocate_window(frame, policy, window)
        self.assertEqual(Replay(self.pulse, c['receiver']).steps(0, 40, Fraction(1, 200)), expected)

    def test_full_pulse_budget_and_post_cutoff_zero(self):
        replay = Replay(self.pulse, self.config(16)['receiver'])
        for key, source in [('energy_j', 'energy_transferred_j'), ('smoke_kg', 'smoke_transferred_kg')]:
            pieces = []
            for first in range(0, 320, 40):
                pieces.append(math.fsum(math.fsum(s[key]) for s in replay.steps(first, 40, Fraction(1, 200))))
            self.assertTrue(math.isclose(math.fsum(pieces), self.pulse['totals'][source], rel_tol=1e-13))
        self.assertTrue(all(v == 0 for s in replay.steps(320, 2, Fraction(1, 200)) for a in s.values() for v in a))

    def test_temporal_overlap_across_donor_frames(self):
        replay = Replay(self.pulse, self.config(16)['receiver'])
        whole = replay.steps(0, 1, Fraction(2, 5))[0]
        split = replay.steps(0, 2, Fraction(1, 5))
        for key in whole:
            self.assertLess(max(abs(a-b-c) for a,b,c in zip(whole[key],split[0][key],split[1][key])), 1e-16)

    def test_conservative_same_physical_source_on_32_and_64(self):
        coarse = Replay(self.pulse, self.config(32)['receiver']).steps(0, 1, Fraction(1, 5))[0]
        fine = Replay(self.pulse, self.config(64)['receiver']).steps(0, 1, Fraction(1, 5))[0]
        for key in coarse:
            restricted = [math.fsum(fine[key][((2*k+dk)*64+2*j+dj)*64+2*i+di]
                                   for dk in (0,1) for dj in (0,1) for di in (0,1))
                          for k in range(32) for j in range(32) for i in range(32)]
            self.assertLess(math.fsum(abs(a-b) for a,b in zip(coarse[key],restricted)),
                            max(math.fsum(coarse[key]), 1e-30)*1e-12)

    def test_pulse_tamper_and_mutation_refused(self):
        bad = copy.deepcopy(self.pulse);bad['totals']['energy_transferred_j'] += 1
        with self.assertRaises(ValueError):validate(bad)
        replay = Replay(self.pulse, self.config(16)['receiver'])
        replay.receiver['spacing_m'][0] *= 2
        with self.assertRaises(ValueError):replay.steps(0, 1, Fraction(1, 200))

    def test_solid_overlap_refused_before_native_work(self):
        receiver = self.config(16)['receiver'];receiver['fluid_mask'] = [0] * 256
        receiver['injection_fluid_mask'] = [0] * len(receiver['injection_fluid_mask'])
        with self.assertRaises(ValueError):Replay(self.pulse, receiver)


if __name__ == '__main__':
    unittest.main()
