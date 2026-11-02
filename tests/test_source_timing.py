"""Separate host-window grouping from actual native Fire emission timing."""
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
from coupling.source_replay import Replay, SCHEMA, TICK_SCHEMA, validate
from coupling.overnight_cases import configuration
from numeric_digest_stream import sealed


class SourceTimingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pulse = json.loads(Path(os.environ['TIMING_PULSE']).read_text())
        cls.original = json.loads(Path(os.environ['TIMING_COARSE_PULSE']).read_text())
        w = json.loads(Path(os.environ['REPLAY_WORKERS']).read_text())
        workers = (w['growth_root'], *[w['binaries'][k]['path'] for k in ('fire','fluid','clock')])
        cls.receiver = configuration(workers, 'gust16')['receiver']

    def test_explicit_tick_profile_and_same_extensive_source(self):
        self.assertEqual(self.pulse['schema'], TICK_SCHEMA)
        self.assertEqual(len(self.pulse['frames']), 80)
        self.assertEqual(self.pulse['end_s'], 1.6)
        validate(self.pulse)
        for key in self.original['totals']:
            self.assertTrue(math.isclose(self.original['totals'][key], self.pulse['totals'][key], rel_tol=1e-13))

    def test_grouping_recovers_donor_spatial_emission(self):
        grouped = Replay(self.pulse, self.receiver, deposition_interval=Fraction(1,5))
        original = Replay(self.original, self.receiver)
        for frame in range(8):
            a = grouped.steps(frame, 1, Fraction(1,5))[0]
            b = original.steps(frame, 1, Fraction(1,5))[0]
            for key in a:
                error = math.fsum(abs(x-y) for x,y in zip(a[key],b[key]))
                self.assertLess(error, max(math.fsum(b[key]),1e-30)*1e-12)

    def test_actual_fine_timing_changes_shape_not_total(self):
        fine = Replay(self.pulse, self.receiver).steps(0,40,Fraction(1,200))
        grouped = Replay(self.pulse,self.receiver,deposition_interval=Fraction(1,5)).steps(0,40,Fraction(1,200))
        a = [math.fsum(s['energy_j']) for s in fine]
        b = [math.fsum(s['energy_j']) for s in grouped]
        self.assertTrue(math.isclose(math.fsum(a),math.fsum(b),rel_tol=1e-13))
        self.assertGreater(math.fsum(abs(x-y) for x,y in zip(a,b))/math.fsum(a),1e-10)

    def test_same_source_is_exact_under_host_partition(self):
        for period in (None,Fraction(1,5)):
            replay=Replay(self.pulse,self.receiver,deposition_interval=period)
            self.assertEqual(replay.steps(0,40,Fraction(1,200)),
                             replay.steps(0,20,Fraction(1,200))+replay.steps(20,20,Fraction(1,200)))

    def test_capacity_and_sequence_refusals(self):
        bad=copy.deepcopy(self.pulse);bad['schema']=SCHEMA
        with self.assertRaises(ValueError):validate(sealed(bad))
        bad=copy.deepcopy(self.pulse);bad['frames'][1]['event']['sequence']=0
        bad['frames'][1]=sealed(bad['frames'][1])
        with self.assertRaises(ValueError):validate(sealed(bad))

    def test_no_fine_emission_claim_from_coarse_frame(self):
        with self.assertRaisesRegex(ValueError,'cut a producer frame'):
            Replay(self.original,self.receiver,deposition_interval=Fraction(1,10))

    def test_policy_mutation_refused(self):
        replay=Replay(self.pulse,self.receiver,deposition_interval=Fraction(1,5))
        replay.deposition_interval=Fraction(1,10)
        with self.assertRaisesRegex(ValueError,'input mutated'):
            replay.steps(0,1,Fraction(1,200))


if __name__=='__main__':
    unittest.main()
