import copy,math,sys,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.prescribed_flow_report import validate,forcing_check,FORCING
from test_flow_spatial_report import native
def fixture():
    rows=[dict(native(n),qualification_profile='prescribed_thermal_flow_v1') for n in (64,128)]
    return dict(schema='prescribed_thermal_flow_native_v1',forcing=FORCING,forcing_steps=640,
                forcing_time_offset_s=0,forcing_sample_time_s=3.195,memory_limit_bytes=2147483648,
                memory_peak_bytes=1000,memory_live_bytes=0,rejected_allocations=0,runs=rows)
class PrescribedFlowReport(unittest.TestCase):
    def test_old_time_original_timing_and_allocation_closure_required(self):
        validate(fixture())
        for key,value in [('forcing','new_time'),('forcing_steps',639),('forcing_time_offset_s',.005),
                          ('forcing_sample_time_s',3.2),('memory_live_bytes',1),('rejected_allocations',1),('memory_peak_bytes',math.nan)]:
            s=fixture();s[key]=value
            with self.assertRaises(ValueError):validate(s)
        for key,value in [('scalar_cycles',640),('scalar_dt_s',.005),('dt_s',.0025),('heat_balance_j',math.nan),('grid',64)]:
            s=fixture();s['runs'][1][key]=value
            with self.assertRaises(ValueError):validate(s)
    def test_intensive_thermal_mapping_and_extensive_restriction(self):
        n=4;c=np.arange(n**3,dtype=float)/7
        f=np.repeat(np.repeat(np.repeat(c.reshape(n,n,n),2,axis=0),2,axis=1),2,axis=2).ravel()/8
        self.assertTrue(forcing_check(c,f,n)['child_values_bit_identical'])
        for value in (math.nan,-1,.01):
            bad=f.copy();bad[0]=value
            with self.assertRaises(ValueError):forcing_check(c,bad,n)
        with self.assertRaises(ValueError):forcing_check(c,f[:-1],n)
if __name__=='__main__':unittest.main()
