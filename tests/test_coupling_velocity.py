import copy
import math
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.velocity import sample_mac,observation,basis_valid
class VelocityTests(unittest.TestCase):
    def test_zero_signed_constant_and_periodic_edges(self):
        for vector in ((0,0,0),(2,-3,.5),(-1,2,-4)):
            field=sum(([v]*64 for v in vector),[])
            for point in ([0,0,.125],[1,1,.875],[.2,.7,.4]):
                for actual,expected in zip(sample_mac(field,[4]*3,[1]*3,point),vector):self.assertAlmostEqual(actual,expected)
    def test_native_staggered_linear_field_interior(self):
        field=[]
        for axis in range(3):
            for k in range(8):
                for j in range(8):
                    for i in range(8):
                        xyz=[(c+(0 if a==axis else .5))/8 for a,c in enumerate((i,j,k))]
                        field.append((axis+1)*xyz[0]-2*xyz[1]+.5*xyz[2])
        point=[.4,.45,.5];actual=sample_mac(field,[8]*3,[1]*3,point)
        for a,value in enumerate(actual):self.assertAlmostEqual(value,(a+1)*point[0]-2*point[1]+.5*point[2],places=14)
    def test_solid_floor_shear_and_open_top(self):
        grid=[4,4,4];field=[speed*((k+.5)/4) for speed in (2,-3) for k in range(4) for _ in range(16)]+[0.]*(64+16)
        for z in (0,.01,.125,.2,.5,.875):
            actual=sample_mac(field,grid,[1]*3,[0,1,z],solid_floor=True)
            self.assertAlmostEqual(actual[0],2*z);self.assertAlmostEqual(actual[1],-3*z);self.assertEqual(actual[2],0)
        self.assertEqual(sample_mac(field,grid,[1]*3,[.3,.7,1],solid_floor=True),[1.75,-2.625,0.])
        # Top is a separate native normal face; it must not wrap to the floor.
        field[2*64:]=[k/4 for k in range(5) for _ in range(16)]
        self.assertAlmostEqual(sample_mac(field,grid,[1]*3,[.3,.7,.99],solid_floor=True)[2],.99)
    def test_solid_floor_observation_identity_and_policy(self):
        cp,cfg=self.checkpoint();f=cp['fluid'];velocity=[2.]*64+[-3.]*64+[0.]*(64+16)
        f['model']='xy_periodic_ground_open_top_projection3d_v1'
        f['fields']['face_velocity_m_s']=velocity;f['state']['data']={'face_velocity_m_s':velocity}
        cfg['fluid']['boundary_policy']={'vertical':'solid_bottom_open_top','predictor_velocity':'no_slip_bottom_zero_gradient_top'}
        result=observation(cp,cfg,8,0,height_m=.01)
        self.assertAlmostEqual(result['tangent_velocity_m_s'][0][0],.16)
        self.assertIn('solid_floor',result['grid_convention'])
        velocity[128]=1
        with self.assertRaises(ValueError):observation(cp,cfg,8,0,height_m=.1)
    def checkpoint(self):
        velocity=[2.]*64+[-3.]*64+[.5]*64
        cp={'digest':'joint','binding_digest':'bound','evidence':{'rate':200,'window_ticks':[0,8]},
            'fluid':{'model':'periodic_evolving_passive3d_v1','fields':{'face_velocity_m_s':velocity},
                     'state':{'digest':'native','data':{'flow_storage':velocity}}}}
        cfg={'fluid':{'grid':[4]*3,'length_m':[1]*3},'source':{'width':2,'height':2,'origin_m':[0,0,0],'cell_size_m':.25}}
        return cp,cfg
    def test_rotated_tangents_and_exact_provenance(self):
        cp,cfg=self.checkpoint();v=observation(cp,cfg,8,0,height_m=.125,basis_u=(0,1,0),basis_v=(-1,0,0))
        self.assertEqual(v['tangent_velocity_m_s'],[[-3.,-2.]]*4)
        self.assertEqual(v['native_state_digest'],'native');self.assertFalse(v['applied_to_fire'])
    def test_age_floor_identity_and_basis_refusals(self):
        cp,cfg=self.checkpoint()
        for tick,age,height in ((7,0,.1),(9,0,.1),(8,0,0),(8,0,1)):
            with self.assertRaises(ValueError):observation(cp,cfg,tick,age,height_m=height)
        self.assertEqual(observation(cp,cfg,9,1,height_m=.1)['request_tick'],9)
        with self.assertRaises(ValueError):basis_valid((1,0,0),(1,0,0))
        cp['fluid']['fields']['face_velocity_m_s']=list(cp['fluid']['fields']['face_velocity_m_s']);cp['fluid']['fields']['face_velocity_m_s'][0]=5
        with self.assertRaises(ValueError):observation(cp,cfg,8,0,height_m=.1)
if __name__=='__main__':unittest.main()
