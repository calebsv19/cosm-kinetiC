"""Bulk enthalpy transport must not be interpreted as surface absorbed heat."""
import copy
import math
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import hotgas_domain as domain
from coupling.hotgas_domain_cases import config,face,conserved
WORKER=os.environ['HOTGAS_DOMAIN_WORKER']


class PlaneSemantics(unittest.TestCase):
    def fixture(self):
        c=config(gravity=(0.,0.,0.),conductivity=.026)
        c['domain']['faces'][4]=face(wall_temperature=300.)
        u=conserved(1.,300.,(0.,0.,2.))*math.prod(c['gas']['grid'])
        source=[0.]*len(u);rate=(1,1000000)
        s=domain.initial(c,WORKER,u,source,rate)
        return c,s,source,rate

    def test_bulk_enthalpy_and_surface_conduction_are_distinct(self):
        c,s,source,rate=self.fixture()
        observation=domain.observe(c,WORKER,s,source,rate,[0.,.013])
        surface,gauge=observation['native_samples']
        for row in (surface,gauge):self.assertTrue(all(abs(v)<1e-10 for v in row['conducted_downward_w_m2']))
        self.assertTrue(all(v==0 for v in surface['advected_upward_w_m2']))
        expected=(287*300/.4+2+287*300)*2
        for v in gauge['advected_upward_w_m2']:self.assertAlmostEqual(v,expected,places=8)
        self.assertFalse(observation['experimental_gauge_response_qualified'])
        self.assertFalse(observation['reciprocal_thermal_exchange_admissible'])

    def test_unresolved_or_unconfigured_plane_refused_without_state_change(self):
        c,s,source,rate=self.fixture();before=copy.deepcopy(s)
        for planes in ([.1],[-.001],[float('nan')]):
            with self.assertRaises(ValueError):domain.observe(c,WORKER,s,source,rate,planes)
            self.assertEqual(s,before)
        c['domain']['faces'][4]=face()
        s=domain.initial(c,WORKER,s['conserved'],source,rate);before=copy.deepcopy(s)
        with self.assertRaises(ValueError):domain.observe(c,WORKER,s,source,rate,[0.,.013])
        self.assertEqual(s,before)


if __name__=='__main__':unittest.main()
