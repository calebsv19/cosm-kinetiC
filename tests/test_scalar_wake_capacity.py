"""Closed larger-grid admission must preserve physical settings and old caps."""
import copy,json,os,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from open_atmosphere import validate,configuration,SCHEMA
class ScalarWakeCapacity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.f=json.loads(Path(os.environ['SCALAR_CAPACITY_CONFIG']).read_text())['fluid']
    def test_explicit_capacity_bound_in_configuration(self):
        r=dict(self.f,schema=SCHEMA,state=None,steps=[]);validate(r);self.assertEqual(configuration(r)['capacity_profile'],'scalar-wake80/v1')
    def test_absent_or_old_profile_does_not_admit_new_geometry(self):
        for profile in (None,'wake80/v1','scalar-wake80/v2'):
            r=dict(self.f,schema=SCHEMA,state=None,steps=[])
            if profile is None:r.pop('capacity_profile')
            else:r['capacity_profile']=profile
            with self.subTest(profile=profile),self.assertRaises(ValueError):validate(r)
    def test_no_physics_drift_or_sparse_mode(self):
        for key,value in [('momentum_dt_s',.0025),('grid',[128]*3),('scalar_transport','bad'),('transport_scheme','muscl_minmod'),('open_domain_forcing',{}),('solid_box',{})]:
            r=dict(self.f,schema=SCHEMA,state=None,steps=[]);r[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):validate(r)
        with self.assertRaises(ValueError):validate(dict(self.f,schema=SCHEMA,state=None,steps=[]),movie=True)
if __name__=='__main__':unittest.main()
