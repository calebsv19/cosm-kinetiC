import math
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.prescribed_pool import disk_cell_area,packet,request
from coupling.methanol_boundary import experimental_boundary

class PrescribedPool(unittest.TestCase):
    def test_circle_support_across_grids(self):
        reference=os.environ['METHANOL_REFERENCE']
        for n in (4,8,16,32):
            p=packet(reference,grid=(n,n,4))
            self.assertAlmostEqual(math.fsum(p['source_cell_areas_m2']),math.pi*(.301/2)**2,places=14)
            self.assertTrue(all(a>=0 for a in p['source_cell_areas_m2']))
        self.assertEqual(disk_cell_area(1,2,1,2,.15),0)

    def test_fuel_energy_distinction_and_budget(self):
        p=packet(os.environ['METHANOL_REFERENCE'])
        self.assertEqual(p['mass_rate_kg_s'],.000932)
        self.assertEqual(p['reference_peak_temperature_k'],1290)
        self.assertFalse(p['fuel_mass_is_smoke'])
        r=request(p)
        self.assertAlmostEqual(math.fsum(r['steps'][0]['energy_j']),.000932*19.9e6*.005)
        self.assertEqual(math.fsum(r['steps'][0]['smoke_kg']),0)
        self.assertFalse(p['full_fuel_admission_qualified'])

    def test_experimental_planes_and_studies_separated(self):
        b=experimental_boundary()
        self.assertAlmostEqual(b['gauge']['height_above_liquid_m'],.013)
        self.assertFalse(b['gauge']['direct_surface_forcing_admissible'])
        self.assertIsNone(b['cooling']['conductance_w_k'])
        self.assertEqual(b['table_3']['mass_flux_kg_m2_s'],.0135)
        self.assertNotEqual(b['case_id'],packet(os.environ['METHANOL_REFERENCE'])['case_id'])

if __name__=='__main__': unittest.main()
