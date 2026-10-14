import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.matched_heat_report import compare, profile, validate_native

def native():
    return dict(schema='matched_heat_v1',velocity_driver_grid=64,flow_dt_s=.005,time_s=3.2,
                control_bit_identical_each_step=True,control_max_difference_j=0,
                driver_scalar_cycles=640,driver_max_divergence=1e-12,
                fine_max_divergence=1e-12,driver_max_pressure_residual=1e-12,
                memory_limit_bytes=2147483648,memory_peak_bytes=1000,memory_live_bytes=0,
                rejected_allocations=0,carriers=[dict(grid=n,scalar_dt_s=dt,
                scalar_cycles=round(3.2/dt),maximum_courant_bound=.2,
                energy_j=10,input_j=10,inflow_j=0,outflow_j=0,heat_balance_j=0,
                peak_temperature_k=310) for n,dt in ((64,.005),(64,.0025),(128,.005),(128,.0025))])

class MatchedHeatReport(unittest.TestCase):
    def test_native_identity_and_explicit_cycles(self):
        validate_native(native())
        for field,value in (('driver_scalar_cycles',641),('fine_max_divergence',float('nan')),
                            ('memory_live_bytes',1),('control_bit_identical_each_step',False)):
            v=native();v[field]=value
            with self.assertRaises(ValueError):validate_native(v)
        v=native();v['carriers'][2]['scalar_cycles']=783
        with self.assertRaises(ValueError):validate_native(v)
        v=native();v['carriers'][2]['heat_balance_j']=1e-6
        with self.assertRaises(ValueError):validate_native(v)

    def test_same_physical_roi_threshold_and_excluded_cells(self):
        n=32;fine=[0.]*(n**3);q=(4*n+12)*n+12;fine[q]=10
        coarse=fine.copy();coarse[q]=9
        # Huge error in an excluded corner cannot alter the declared gate.
        coarse[-1]=1000
        result=compare(coarse,fine,n)
        self.assertEqual(result['local_heat_relative_l1'],.1)
        self.assertTrue(result['local_heat_gate_passed'])
        coarse[q]=8.99
        self.assertFalse(compare(coarse,fine,n)['local_heat_gate_passed'])

    def test_profile_observes_extensive_si_on_fixed_slabs(self):
        for n in (32,64):
            heat=[1.2*1005*2/n**3]*(n**3)
            for value in profile(heat,n):self.assertAlmostEqual(value,2)

    def test_zero_signal_is_not_false_pass(self):
        n=32;fine=[0.]*(n**3);coarse=fine.copy();coarse[(4*n+12)*n+12]=1
        result=compare(coarse,fine,n)
        self.assertIsNone(result['local_heat_relative_l1'])
        self.assertFalse(result['local_heat_gate_passed'])

if __name__=='__main__':unittest.main()
