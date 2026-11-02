"""Recipes vary one named clock while preserving native/source/object physics."""
from fractions import Fraction
import json
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.timing_cases import CASES,configuration


class TimingCasesTests(unittest.TestCase):
    def test_fixed_native_clocks_budget_and_physics(self):
        w=json.loads(Path(os.environ['REPLAY_WORKERS']).read_text())
        workers=(w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')])
        configs=[configuration(workers,name) for name in CASES]
        for c in configs:
            exchange=Fraction(*c['periods'][2])
            self.assertEqual(exchange*c['windows'],Fraction(16,5))
            self.assertEqual(exchange*c['source_delivery']['active_windows'],Fraction(8,5))
            self.assertEqual(c['periods'][:2],[[1,50],[1,200]])
            self.assertEqual(c['native_packet_steps'],20)
            for key in ('source','fluid','receiver'):
                self.assertEqual(c[key],configs[0][key])


if __name__=='__main__':unittest.main()
