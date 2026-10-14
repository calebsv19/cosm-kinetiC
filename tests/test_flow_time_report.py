import math
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.flow_time_report import validate, relative_l2, pair

def native():
    return dict(qualification_profile='flow_time_64_v1',grid=64,dt_s=.005,time_s=3.2,
        scalar_dt_s=.00125,flow_steps=640,scalar_cycles=2560,scalar_work_cells=2560*64**3,
        scalar_transport_scheme='muscl_minmod',maximum_divergence=1e-12,
        maximum_pressure_residual=1e-12,heat_balance_j=0,smoke_balance_kg=0,
        peak_temperature_k=310,memory_limit_bytes=2147483648,memory_peak_bytes=1000)

class FlowTimeReport(unittest.TestCase):
    def test_reject_timing_and_resource_drift(self):
        validate(native(),.005)
        for key,val in [('scalar_cycles',640),('scalar_dt_s',.0025),('flow_steps',1280),
                        ('maximum_divergence',math.nan),('heat_balance_j',1e-8),
                        ('maximum_pressure_residual',1e-10),('memory_peak_bytes',2147483649)]:
            s=native();s[key]=val
            with self.assertRaises(ValueError):validate(s,.005)

    def test_velocity_difference_uses_fine_signal(self):
        self.assertEqual(relative_l2([.9,0],[1,0])['relative_l2'],1-.9)
        self.assertIsNone(relative_l2([1,0],[0,0])['relative_l2'])
        with self.assertRaises(ValueError):relative_l2([1],[1,2])

    def test_temporal_heat_gate_keeps_physical_region_and_threshold(self):
        n=32;count=n**3;q=(4*n+12)*n+12
        b=dict(energy_j=[0.]*count,smoke_kg=[0.]*count,
               face_velocity_m_s=[.01]*(3*count+3*n*n))
        b['energy_j'][q]=10;b['smoke_kg'][q]=1e-5
        a={key:values.copy() for key,values in b.items()};a['energy_j'][q]=9
        # A large excluded corner difference cannot change the heat region.
        a['energy_j'][-1]=1000
        sa=dict(dt_s=.005,peak_temperature_k=310);sb=dict(dt_s=.0025,peak_temperature_k=310)
        result=pair(a,b,sa,sb,n)
        self.assertEqual(result['heat']['local_heat_relative_l1'],.1)
        self.assertTrue(result['heat']['local_heat_gate_passed'])
        # The independently computed profile may round just above 0.1 at
        # the exact boundary; test combined screens with a clear margin.
        a['energy_j'][q]=9.01
        self.assertTrue(pair(a,b,sa,sb,n)['temporal_screens_passed'])
        a['energy_j'][q]=8.99
        self.assertFalse(pair(a,b,sa,sb,n)['temporal_screens_passed'])

if __name__=='__main__':unittest.main()
