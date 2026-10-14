import unittest
import numpy as np
from coupling.horizontal_reference_report import validate,restrict_x,heat_compare
from coupling.vertical_reference_report import dense
from coupling.flow_spatial_fields import pack
class HorizontalTests(unittest.TestCase):
 def row(self):
  return dict(qualification_profile='flow_horizontal_matched_0025_v1',grid=[256,128,256],dt_s=.0025,time_s=3.2,scalar_dt_s=.00125,flow_steps=1280,scalar_cycles=2560,scalar_work_cells=2560*256*128*256,max_scalar_work_cells=22000000000,scalar_transport_scheme='muscl_minmod',memory_limit_bytes=8589934592,memory_peak_bytes=1000,maximum_divergence=0,maximum_pressure_residual=0,heat_balance_j=0,smoke_balance_kg=0,peak_temperature_k=310)
 def test_identity_and_invariants(self):
  validate(self.row())
  for k,v in [('grid',[256]*3),('dt_s',.005),('scalar_dt_s',.0025),('flow_steps',640),('scalar_work_cells',1),('maximum_pressure_residual',1e-10),('heat_balance_j',1e-8),('memory_peak_bytes',8589934593)]:
   with self.assertRaises(ValueError):validate(dict(self.row(),**{k:v}))
 def test_extensive_and_face_area_restriction(self):
  grid=[8,4,8];x,y,z=grid
  f=[np.arange(z*y*(x+1),dtype=float).reshape(z,y,x+1),np.arange(z*(y+1)*x,dtype=float).reshape(z,y+1,x),np.arange((z+1)*y*x,dtype=float).reshape(z+1,y,x)]
  values=np.arange(x*y*z,dtype=float);a=dict(energy_j=values,smoke_kg=values/100,face_velocity_m_s=pack(f));b=restrict_x(a,grid)
  self.assertEqual(a['energy_j'].sum(),b['energy_j'].sum())
  g=dense(b['face_velocity_m_s'],[4,4,8]);np.testing.assert_array_equal(g[0],f[0][:,:,::2]);np.testing.assert_array_equal(g[1],(f[1][:,:,::2]+f[1][:,:,1::2])/2);np.testing.assert_array_equal(g[2],(f[2][:,:,::2]+f[2][:,:,1::2])/2)
 def test_same_physical_heat_region(self):
  grid=[8,8,32];b=np.ones(8*8*32);a=b.copy();a.reshape(32,8,8)[0,0,0]=1000
  self.assertEqual(heat_compare(a,b,grid)['local_heat_relative_l1'],0)
  a.reshape(32,8,8)[:12,2:6,2:6]*=1.2
  self.assertFalse(heat_compare(a,b,grid)['local_heat_gate_passed'])
if __name__=='__main__':unittest.main()
