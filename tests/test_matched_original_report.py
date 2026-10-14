import copy,math,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.matched_original_report import validate

def native():
 rows=[dict(grid=n,scalar_dt_s=.00125,scalar_cycles=2560,heat_balance_j=0,maximum_courant_bound=.1,peak_temperature_k=310,energy_j=1,input_j=1,inflow_j=0,outflow_j=0) for n in (64,128)]
 return dict(schema='matched_original_source_v1',velocity_driver_grid=64,flow_dt_s=.005,time_s=3.2,driver_scalar_cycles=2560,control_bit_identical_each_step=True,control_max_difference_j=0,driver_max_divergence=1e-12,fine_max_divergence=1e-12,driver_max_pressure_residual=1e-12,memory_limit_bytes=2147483648,memory_peak_bytes=1000,memory_live_bytes=0,rejected_allocations=0,carriers=rows)
class MatchedOriginalReport(unittest.TestCase):
 def test_source_time_control_and_finite_accounting_are_required(self):
  validate(native())
  for key,value in [('driver_scalar_cycles',640),('control_bit_identical_each_step',False),('fine_max_divergence',math.nan),('memory_live_bytes',1)]:
   s=native();s[key]=value
   with self.assertRaises(ValueError):validate(s)
  for key,value in [('scalar_dt_s',.0025),('scalar_cycles',1280),('input_j',math.nan),('maximum_courant_bound',.401)]:
   s=native();s['carriers'][1][key]=value
   with self.assertRaises(ValueError):validate(s)
if __name__=='__main__':unittest.main()
