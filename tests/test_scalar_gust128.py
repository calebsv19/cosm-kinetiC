"""Closed normal128 policy, source embedding and separately selected storage."""
import copy,json,os,sys,unittest,zlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.scalar_gust128 import validate,PROFILE,NUMERICAL_BYTES,Source
from coupling.journal import Journal,FIELD_PACKET,FIELD_PACKET_2G
class Gust128(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.f=json.loads(Path(os.environ['GUST128_POLICY']).read_text())
    def test_closed_profile_and_packet_horizon(self):
        r=dict(self.f,state=None,steps=[{}]*4);validate(r)
        for update in ({'steps':[{}]*5},{'state':{'data':{'steps':98}},'steps':[{}]*4},{'capacity_profile':'scalar-wake80/v1'},{'grid':[80]*3}):
            with self.subTest(update=update),self.assertRaises(ValueError):validate(dict(r,**update))
        validate(dict(r,state={'data':{'steps':100}},steps=[]))
    def test_physics_and_resource_drift_refused(self):
        for key,field,value in [('resource_limits','numerical_bytes',1536*1024**2),('resource_limits','max_cells',128**3+1),('resource_limits','native_request_bytes',512*1024**2),('properties','conductivity_w_m_k',.027),('inert_object','h_w_m2_k',2.1),('buoyancy','enabled',False),('open_domain_forcing','response_s',.3)]:
            r=copy.deepcopy(self.f);r[key][field]=value
            with self.subTest(key=key,field=field),self.assertRaises(ValueError):validate(r)
        for key,value in [('momentum_dt_s',.0025),('scalar_transport','other'),('transport_scheme','muscl_minmod')]:
            with self.subTest(key=key),self.assertRaises(ValueError):validate(dict(self.f,**{key:value}))
    def test_separate_journal_capacity_same_encoding(self):
        value={'field':[1./7]*300};a=Journal(FIELD_PACKET);b=Journal(FIELD_PACKET_2G)
        self.assertEqual(a.encode(value),b.encode(value));self.assertEqual(b.decode(a.encode(value)),value)
        self.assertEqual(a.limit,1024**3);self.assertEqual(b.limit,2*1024**3)
        with self.assertRaises(ValueError):b.decode(b.encode(value)+b'x')
    def test_source_geometry_refuses_before_mapping(self):
        receiver=dict(dimensions=[128]*3,spacing_m=[1/128]*3,origin_m=[0]*3,layer=0,injection_depth_m=.125)
        for key,value in [('injection_depth_m',.3),('spacing_m',[1/64]*3),('origin_m',[0,0,.1])]:
            with self.subTest(key=key),self.assertRaises(ValueError):Source({},dict(receiver,**{key:value}))
if __name__=='__main__':unittest.main()
