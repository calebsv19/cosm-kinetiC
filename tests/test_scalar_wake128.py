"""Long profile is separate from the short/default admission; no native calls."""
import copy,json,os,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import scalar_gust128 as short,scalar_wake128 as long
class Wake128Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.policy=json.loads(Path(os.environ['GUST128_POLICY']).read_text())
    def test_old_horizon_remains_closed(self):
        r=dict(self.policy,state={'data':{'steps':100}},steps=[]);short.validate(r)
        with self.assertRaises(ValueError):short.validate(dict(r,steps=[{}]))
        with self.assertRaises(ValueError):short.validate(dict(r,capacity_profile=long.PROFILE))
    def test_new_horizon_and_two_step_packets(self):
        r=dict(self.policy,capacity_profile=long.PROFILE,state={'data':{'steps':638}},steps=[{},{}]);long.validate(r)
        long.validate(dict(r,state={'data':{'steps':640}},steps=[]))
        for update in ({'steps':[{}]*3},{'state':{'data':{'steps':639}}},{'state':{'data':{'steps':-1}}},{'state':{'data':{'steps':1.5}}},{'capacity_profile':short.PROFILE}):
            with self.subTest(update=update),self.assertRaises(ValueError):long.validate(dict(r,**update))
    def test_fixed_physics_and_budget(self):
        r=dict(self.policy,capacity_profile=long.PROFILE,state=None,steps=[])
        for section,key,value in [('resource_limits','numerical_bytes',3*1024**3),('resource_limits','native_output_bytes',512*1024**2),('inert_object','h_w_m2_k',2.1),('open_domain_forcing','response_s',.21)]:
            bad=copy.deepcopy(r);bad[section][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):long.validate(bad)
        with self.assertRaises(ValueError):long.validate(dict(r,momentum_dt_s=.0025))
    def test_source_pulse_and_geometry_before_mapping(self):
        receiver=dict(dimensions=[128]*3,spacing_m=[1/128]*3,origin_m=[0]*3,layer=0,injection_depth_m=.125)
        with self.assertRaises(ValueError):long.Source({'digest':'wrong'},receiver)
        with self.assertRaises(ValueError):long.Source({'digest':long.PULSE},dict(receiver,injection_depth_m=.25))
    def test_old_runner_cannot_admit_long_history(self):
        from coupling.transport_replay import ReplayRun
        with self.assertRaisesRegex(ValueError,'bounded retention'):
            ReplayRun('/unused',{'fluid':{'capacity_profile':long.PROFILE},'fluid_worker':'/unused'},None,packet_steps=2)
if __name__=='__main__':unittest.main()
