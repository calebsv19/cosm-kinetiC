"""Opt-in lateral admission, centered held wind and exact native carrier boundaries."""
import copy,json,os,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.isolated_qualification import configuration,checks
from coupling.open_boundaries import layout,face
from coupling.framework import Framework
from coupling.playback_export import export_playback
from open_atmosphere import validate,SCHEMA

class IsolatedTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='fire-fluid-isolated-test-');self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        previous=os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT');os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(self.root/'evidence')
        def restore():
            if previous is None:os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT',None)
            else:os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=previous
        self.addCleanup(restore)
        self.workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
    def test_lateral_layout_and_closed_admission(self):
        config=configuration(self.workers,duration=.3);r=dict(config['fluid'],schema=SCHEMA,state=None,steps=[]);validate(r)
        self.assertEqual(layout(r),(1728,384))
        for key,value in [('pressure_datum_pa',[0,1]),('inflow_smoke_concentration_kg_m3',[0,1]),('horizontal','unknown')]:
            bad=copy.deepcopy(r);bad['boundary_policy'][key]=value
            with self.assertRaises(ValueError):validate(bad)
        bad=copy.deepcopy(r);bad['initial_face_velocity_m_s']=bad['initial_face_velocity_m_s'][:1600]
        with self.assertRaises(ValueError):validate(bad)
    def test_centered_box_wind_and_exact_carrier(self):
        config=configuration(self.workers,box=True,duration=.3);host=Framework(config,self.root/'store');host.initialize()
        for revision in range(3):host.step(revision,'step-'+str(revision))
        cp=host.inspect();report=checks(config,cp['fluid']);self.assertEqual(report['max_blocked_normal_velocity_m_s'],0)
        self.assertTrue(cp['evidence']['held_return']['applied_to_fire']);self.assertEqual(cp['fluid']['model'],'open_xy_ground_box_open_top_projection3d_v1')
        exported=export_playback(host,self.root/'export',revisions=[3]);directory=Path(exported['output'])/'revision-003'
        from combined_scene import HEADER,validate as validate_scene
        validate_scene(directory)
        import struct
        raw=(directory/'frame_000000.vf3d').read_bytes();n=512;velocity=cp['fluid']['fields']['face_velocity_m_s']
        # Independently read the last X cell's upper-face average from the carrier.
        q=7;i=8;j=k=0;expected=.5*(velocity[q]+velocity[face([8]*3,0,i,j,k,True)])
        actual=struct.unpack_from('<f',raw,HEADER.size+4*n+4*q)[0];self.assertAlmostEqual(actual,expected,delta=1e-8)
        self.assertEqual(raw[-512:],bytes(cp['fluid']['fields']['solid_mask']))
        original=host.path.read_bytes();host.step(2,'step-2');self.assertEqual(original,host.path.read_bytes())
if __name__=='__main__':unittest.main()
