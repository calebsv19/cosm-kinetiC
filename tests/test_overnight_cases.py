"""Physical intervention isolation and admitted bound/cadence checks."""
import os,sys,unittest
from pathlib import Path
from fractions import Fraction
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.overnight_cases import configuration,CASES
from coupling.recovery_cases import configuration as recovery
from coupling import fluid_adapter,driven_wind,source_delivery
from coupling.solid_box import topology
class OvernightCases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
    def test_every_recipe_admits_and_preserves_heat_law_and_cutoff(self):
        baseline=recovery(self.workers,'continuous-16')
        for name in CASES:
            c=configuration(self.workers,name);f=c['fluid'];source_delivery.validate(c);fluid_adapter.validate(f)
            self.assertEqual(c['source'],baseline['source']);self.assertEqual(f['inert_object'],baseline['fluid']['inert_object'])
            self.assertEqual(c['windows']*Fraction(*c['periods'][2]),Fraction(32,5))
            self.assertEqual(c['source_delivery']['active_windows']*Fraction(*c['periods'][2]),Fraction(8,5))
            n=f['grid'][0];self.assertEqual(c['receiver']['injection_fluid_mask'],[1-v for v in topology(f)[:n*n*(n//8)]])
            self.assertLessEqual(f['resource_limits']['numerical_bytes'],512*1024*1024)
    def test_reversal_is_paired_with_same_boundary_and_absolute_clock(self):
        for n in (16,32):
            a=configuration(self.workers,'balanced'+str(n));b=configuration(self.workers,'reverse'+str(n));w=b['fluid']['wind_forcing']
            self.assertEqual(driven_wind.command(b['fluid'],479),.5);self.assertEqual(driven_wind.command(b['fluid'],480),-.5)
            w['knots']=a['fluid']['wind_forcing']['knots'];self.assertEqual(a,b)
    def test_spatial_pair_matches_physics_and_cadence(self):
        a=configuration(self.workers,'exchange32');b=configuration(self.workers,'spatial64')
        self.assertEqual(a['periods'],b['periods']);self.assertEqual(a['source_delivery'],b['source_delivery'])
        for k in ('properties','length_m','buoyancy','wind_forcing','solid_box','inert_object','momentum_dt_s'):self.assertEqual(a['fluid'][k],b['fluid'][k])
    def test_buoyancy_control_changes_only_switch(self):
        for name in ('no-buoyancy16','windoff-no-buoyancy16'):
            c=configuration(self.workers,name);b=configuration(self.workers,'windoff16') if name.startswith('windoff') else recovery(self.workers,'continuous-16')
            b['windows']=16
            for key in ('native_request_bytes','native_output_bytes'):b['fluid']['resource_limits'][key]=c['fluid']['resource_limits'][key]
            c['fluid']['buoyancy']['enabled']=True;self.assertEqual(c,b)
if __name__=='__main__':unittest.main()
