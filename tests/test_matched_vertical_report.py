import unittest
from coupling.matched_vertical_report import validate
from coupling.vertical_reference_report import validate as old_validate
class MatchedVerticalTests(unittest.TestCase):
 def row(self):
  return dict(qualification_profile='flow_vertical_matched_0025_v1',grid=[128,128,256],dt_s=.0025,time_s=3.2,scalar_dt_s=.00125,flow_steps=1280,scalar_cycles=2560,scalar_work_cells=2560*128*128*256,max_scalar_work_cells=12000000000,scalar_transport_scheme='muscl_minmod',maximum_divergence=0,maximum_pressure_residual=0,heat_balance_j=0,smoke_balance_kg=0,peak_temperature_k=310,memory_limit_bytes=4294967296,memory_peak_bytes=1000)
 def test_matched_admission_and_alias_refusal(self):
  row=self.row();validate(row)
  for key,value in [('dt_s',.005),('flow_steps',640),('scalar_cycles',1280),('grid',[128]*3),('time_s',3.09)]:
   with self.assertRaises(ValueError):validate(dict(row,**{key:value}))
 def test_old_five_ms_reader_stays_strict(self):
  with self.assertRaises(ValueError):old_validate(self.row())
 def test_conservation_and_pressure_bounds_stay_strict(self):
  for key,value in [('heat_balance_j',1e-8),('maximum_pressure_residual',1e-10),('maximum_divergence',1e-7),('memory_peak_bytes',4294967297)]:
   with self.assertRaises(ValueError):validate(dict(self.row(),**{key:value}))
if __name__=='__main__':unittest.main()
