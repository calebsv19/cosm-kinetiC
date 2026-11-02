"""Independent interpolation and conservative restriction for gust reports."""
import sys,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.gust_diagnostics import sample,points,restricted,compare
from coupling.velocity import sample_mac
class GustDiagnosticsTests(unittest.TestCase):
 def test_native_sampler_parity(self):
  for n in (8,16):
   v=np.random.default_rng(42).normal(size=3*n**3+3*n*n);p=points()[::37];actual=sample(v,n,p)
   expected=[sample_mac(v.tolist(),[n]*3,[1]*3,list(q),solid_floor=True,lateral_open=True) for q in p]
   np.testing.assert_allclose(actual,expected,rtol=1e-13,atol=1e-13)
 def test_nested_conservation(self):
  n=32;f={'energy_j':np.ones(n**3),'smoke_kg':np.ones(n**3)*.2,'face_velocity_m_s':np.ones(3*n**3+3*n*n)};r=restricted(f,32,8)
  self.assertEqual(r['energy_j'].sum(),f['energy_j'].sum());np.testing.assert_allclose(r['face_velocity_m_s'],1)
  self.assertLess(compare(r,f,8,32)['fire_plane_wind']['rms_difference'],1e-14)
 def test_refuse_unsupported_samples(self):
  with self.assertRaises(ValueError):sample(np.ones(3*8**3+3*8*8),8,[[.5,.5,0]])
if __name__=='__main__':unittest.main()
