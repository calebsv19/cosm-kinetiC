import unittest
import numpy as np
from coupling.reconstructed_flow_report import reconstruction,forcing_check,validate_fine
class TransferReportTests(unittest.TestCase):
 def test_conservative_reference_and_corrupt_child(self):
  n=8;z,y,x=np.indices((n,n,n));c=(2+x+2*y+3*z).ravel().astype(float)
  f=reconstruction(c,n);v=forcing_check(c,f,n);self.assertLess(v['restriction_max_relative'],3e-15)
  f[0]+=1e-5
  with self.assertRaises(ValueError):forcing_check(c,f,n)
 def test_sharp_support_bounds_and_admission(self):
  c=np.zeros((8,8,8));c[2:6,2:6,0:2]=1
  f=reconstruction(c.ravel(),8);self.assertGreaterEqual(f.min(),0);self.assertLessEqual(f.max(),.125)
  c[0,0,0]=np.nan
  with self.assertRaises(ValueError):reconstruction(c.ravel(),8)
 def test_fine_timing_cannot_alias_coarse_counts(self):
  row=dict(qualification_profile='flow_time_128_v1',grid=128,dt_s=.0025,time_s=3.2,scalar_dt_s=.00125,flow_steps=1280,scalar_cycles=2560,scalar_work_cells=2560*128**3,max_scalar_work_cells=8000000000,scalar_transport_scheme='muscl_minmod',maximum_divergence=0,maximum_pressure_residual=0,heat_balance_j=0,smoke_balance_kg=0,peak_temperature_k=310,memory_limit_bytes=2147483648,memory_peak_bytes=100)
  validate_fine(row,.0025)
  for key,value in [('flow_steps',640),('scalar_cycles',1280),('dt_s',.005),('scalar_dt_s',.0025),('maximum_pressure_residual',1e-8)]:
   with self.assertRaises(ValueError):validate_fine(dict(row,**{key:value}),.0025)
if __name__=='__main__':unittest.main()
