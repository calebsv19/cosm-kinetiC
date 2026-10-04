"""Acceptance boundary and observed sampler-missed peak controls."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_domain_budget import enforce_phase,PhaseResourceStopped,RSS_CAP,WALL_CAP


class Budget(unittest.TestCase):
    def test_original_boundary_and_observed_peak_stop(self):
        enforce_phase('factor_ready',RSS_CAP-1,WALL_CAP-1e-6)
        for rss,wall in ((RSS_CAP,1.),(1,WALL_CAP),(1969815552,11.230957)):
            with self.assertRaises(PhaseResourceStopped) as stopped:enforce_phase('factor_ready',rss,wall)
            self.assertEqual(stopped.exception.record['phase'],'factor_ready')
            self.assertEqual(stopped.exception.record['rss_cap_bytes'],1800*1024**2)
    def test_known_overrun_cannot_enter_solve_or_publish(self):
        events=[]
        with self.assertRaises(PhaseResourceStopped):
            enforce_phase('factor_ready',1969815552,11.230957)
            events.append('solve')
            events.append('publish')
        self.assertEqual(events,[])


if __name__=='__main__':unittest.main()
