import copy,sys,unittest
from fractions import Fraction
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.qualification import case,matrix
class QualificationTests(unittest.TestCase):
    def base(self):return {'periods':[[1,50],[1,200],[1,25]],'return_channel':{'sample_height_m':.0625},'fluid':{'grid':[8,8,4],'length_m':[1,1,.5]},'receiver':{'spacing_m':[.125]*3,'dimensions':[8,8,4]}}
    def test_matrix_bound_and_fixed_sensitivity_horizon(self):
        b=self.base();original=copy.deepcopy(b);rows=matrix(b);self.assertEqual(b,original)
        for label,cfg in rows:
            self.assertLessEqual(cfg['windows'],32)
            duration=Fraction(*cfg['periods'][2])*cfg['windows']
            if label in ('reference','height-half','height-double','exchange-half','exchange-slow','strong-wind'):self.assertEqual(duration,Fraction(2,5))
    def test_taller_domain_preserves_spacing_and_shear(self):
        b=self.base();short=case(b);tall=case(b,domain_height=1.)
        self.assertEqual(tall['fluid']['grid'],[8,8,8]);self.assertEqual(tall['receiver']['spacing_m'],short['receiver']['spacing_m'])
        self.assertEqual(tall['fluid']['initial_face_velocity_m_s'][:256],short['fluid']['initial_face_velocity_m_s'][:256])
    def test_alignment_height_and_window_cap_refusals(self):
        for options in ({'exchange':Fraction(1,20)},{'duration':Fraction(10)},{'height':0},{'height':.5},{'domain_height':.6}):
            with self.assertRaises(ValueError):case(self.base(),**options)
if __name__=='__main__':unittest.main()
