"""Refinement recipes must preserve physical inputs rather than cell counts."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.wind_sensitivity import refined,cadence,field_center

class SensitivityRecipe(unittest.TestCase):
    def base(self):
        return {'source':{'physical':'fixed'},'periods':[[1,50],[1,200],[1,10]],'windows':32,
            'fluid':{'length_m':[1,1,1],'grid':[8]*3,'boundary_policy':{'horizontal':'ambient_open_xy'},
            'initial_face_velocity_m_s':[0.]*1728,'initial_energy_j':[0.]*512,'initial_smoke_kg':[0.]*512,
            'wind_forcing':{'knots':[{'step':80,'speed_m_s':.5}]},'solid_box':{'lower_m':[.375]*3,'upper_m':[.625]*3}},
            'receiver':{'injection_depth_m':.125,'spacing_m':[.125]*3,'dimensions':[8]*3,'fluid_mask':[1]*64,'injection_fluid_mask':[1]*64}}
    def test_grid_preserves_geometry_schedule_and_source(self):
        a=self.base();saved=copy.deepcopy(a);b=refined(a,16)
        self.assertEqual(a,saved)
        for key in ('source','periods','windows'):self.assertEqual(a[key],b[key])
        for key in ('wind_forcing','solid_box','length_m'):self.assertEqual(a['fluid'][key],b['fluid'][key])
        self.assertEqual(len(b['fluid']['initial_face_velocity_m_s']),3*16**3+3*16**2)
        self.assertEqual(len(b['receiver']['injection_fluid_mask']),16*16*2)
    def test_nonzero_initial_state_refused(self):
        a=self.base();a['fluid']['initial_smoke_kg'][0]=1
        with self.assertRaises(ValueError):refined(a,16)
    def test_exchange_keeps_native_clocks_and_schedule(self):
        a=self.base();b=cadence(a,'0.04','1.2')
        self.assertEqual(b['windows'],30);self.assertEqual(b['periods'][2],[1,25])
        self.assertEqual(a['periods'][:2],b['periods'][:2]);self.assertEqual(a['fluid'],b['fluid'])
    def test_unaligned_exchange_refused(self):
        with self.assertRaisesRegex(ValueError,'aligned native cadence'):cadence(self.base(),'0.05','1.6')
    def test_uniform_centroid_is_independent_of_grid(self):
        for n in (8,16,32):
            c=self.base();c['fluid']['grid']=[n]*3
            values=[1/n**3]*n**3
            self.assertEqual(field_center({'smoke_kg':values},c,'smoke_kg'),[.5]*3)
    def test_empty_centroid_has_no_invented_position(self):
        self.assertIsNone(field_center({'smoke_kg':[0]*512},self.base(),'smoke_kg'))
    def test_unbounded_exchange_matrix_refused(self):
        with self.assertRaises(ValueError):cadence(self.base(),'0.05','3.2')

if __name__=='__main__':unittest.main()
