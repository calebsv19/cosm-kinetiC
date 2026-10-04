import collections
import hashlib
import json
import math
from pathlib import Path
import sys
import subprocess
import tempfile
import time
import unittest
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service, SessionError
from qualification import mesh, write_shape, reference, FLUIDS
from protocol import call
from wake_qualification import steady_screen, refinement_screen
import copy

class QualificationTests(unittest.TestCase):
    def test_closed_oriented_meshes_and_stl_parity(self):
        for shape in ('sphere','cube','cone'):
            vertices,faces=mesh(shape);edges=collections.Counter();volume=0
            for face in faces:
                for a,b in zip(face,face[1:]+face[:1]):edges[a,b]+=1
                a,b,c=[vertices[i] for i in face]
                volume+=(a[0]*(b[1]*c[2]-b[2]*c[1])+a[1]*(b[2]*c[0]-b[0]*c[2])+a[2]*(b[0]*c[1]-b[1]*c[0]))/6
            for (a,b),count in edges.items():self.assertEqual(count,1);self.assertEqual(edges[b,a],1)
            expected={'cube':1,'sphere':math.pi/6,'cone':math.pi/12}[shape]
            self.assertLess(abs(volume-expected)/expected,.025)
            with tempfile.TemporaryDirectory() as tmp:
                hashes=write_shape(Path(tmp),shape)
                raw=(Path(tmp)/'assets/shape.stl').read_text()
                stl_vertices=[tuple(map(float,line.split()[1:])) for line in raw.splitlines() if line.startswith('vertex ')]
                self.assertEqual(stl_vertices,[vertices[i] for f in faces for i in f])
                self.assertEqual(len(hashes),2)

    def test_references_have_regime_and_no_fake_measured_drag(self):
        low=reference('sphere',.1,.0001,1000,1)
        self.assertAlmostEqual(low['reynolds_number'],.01)
        self.assertAlmostEqual(low['target_drag_coefficient'],2400)
        self.assertAlmostEqual(low['target_drag_n'],3*math.pi*1*.0001*.1)
        for shape in ('cube','cone','sphere'):
            high=reference(shape,.1,2,1.2,1.8e-5)
            self.assertIsNone(high['measured_drag_coefficient']);self.assertIsNone(high['target_drag_coefficient'])

    def test_actual_mesh_worker_and_fluid_contract(self):
        with tempfile.TemporaryDirectory(prefix='s3-agent-') as tmp:
            s=Service(tmp)
            footprints={}
            for shape in ('sphere','cube','cone'):
                scene=call(s,'scene_create',dict(scene_id=shape,template='wind_stl_'+shape,object_size_m=.3))
                rev=scene['scene_revision'];run=shape+'-run'
                settings=dict(grid=[64,32,32],fluid={'density_kg_m3':1000,'dynamic_viscosity_pa_s':1},qualification_mode=True,solver_iterations=48)
                valid=call(s,'scene_validate',dict(scene_id=shape,scene_revision=rev,**settings))
                self.assertAlmostEqual(valid['physics']['kinematic_viscosity_m2_s'],.001)
                self.assertEqual(valid['physics']['solver_iterations_applied'],48)
                self.assertEqual(len(valid['geometry']),1)
                self.assertGreater(valid['geometry'][0]['triangle_count'],0)
                call(s,'run_start',dict(request_id=run,scene_id=shape,scene_revision=rev,steps=20,**settings))
                try:
                    receipt=s.run_control(run,'step','step',rev,5000);self.assertEqual(receipt['status'],'applied')
                    snap=s.run_inspect(run);self.assertEqual(snap['model'],'wind_numerical_qualification_v1')
                    self.assertFalse(snap['physics']['synthetic_wind_enabled']);self.assertIsNone(snap['physics']['drag_coefficient'])
                    self.assertIn('pressure_residual_linf_s_inv',snap['health'])
                    self.assertEqual(snap['health']['projection_operator'], 'prescribed_inlet_transpose_cg_v2')
                    self.assertAlmostEqual(snap['health']['max_divergence'],
                        snap['health']['pressure_residual_linf_s_inv'], delta=2e-5)
                    balance=snap['health']['conservation']
                    self.assertTrue(balance['available'])
                    self.assertAlmostEqual(balance['net_outward_volume_flux_m3_s'], balance['integrated_divergence_m3_s'], delta=1e-9)
                    for port,position in [('inlet',0),('outlet',1)]:
                        port_sample=s.run_sample(run,port,plane='YZ',position=position,resolution=32,field='solid',wait_ms=5000)
                        self.assertGreater(sum(c[2]<.5 for c in port_sample['preview']['samples']),500)
                    sample=s.run_sample(run,'solid',plane='YZ',position=.4,resolution=32,field='solid',wait_ms=5000)
                    cells=sample['preview']['samples'];self.assertGreater(sum(c[2] for c in cells),0)
                    footprints[shape]=[c[2] for c in cells]
                    self.assertEqual(s.run_inspect(run)['health']['export_materializations'],0)
                    self.assertEqual(s.run_inspect(run)['tick'],1)
                    req=json.loads((s.run_dir(run)/'request.json').read_text())
                    self.assertEqual(len(req['geometry_assets']),2)
                    for name,digest in req['geometry_assets'].items():
                        self.assertEqual(hashlib.sha256((s.run_dir(run)/name).read_bytes()).hexdigest(),digest)
                    with self.assertRaises(SessionError):s.run_start(run,shape,rev,steps=20,**dict(settings,solver_iterations=24))
                finally:
                    s.run_control(run,'finish','cancel',rev,5000)
                    s.children[run].wait(timeout=10)
                result=s.run_result(run)
                self.assertTrue(any(a['path'].endswith('.stl') for a in result['artifacts']))
            self.assertNotEqual(footprints['cube'],footprints['sphere'])
            self.assertNotEqual(footprints['cube'],footprints['cone'])
            asset=Path(tmp)/'scenes/cone/assets/shape.runtime.json';asset.write_text('{}')
            with self.assertRaisesRegex(SessionError,'asset_digest'):s.scene_validate('cone',rev)

    def test_sweep_cli_repeat_comparison(self):
        with tempfile.TemporaryDirectory(prefix='s3-sweep-') as tmp:
            a=Path(tmp)/'a';b=Path(tmp)/'b'
            command=[sys.executable,'-B',str(ROOT/'scripts/qualify_fluid.py'),
                     '--shapes','cube','--grids','32','--duration','.02']
            subprocess.run(command+['--output',str(a)],check=True,capture_output=True,timeout=30)
            subprocess.run(command+['--output',str(b),'--compare',str(a/'summary.json')],
                           check=True,capture_output=True,timeout=30)
            diff=json.loads((b/'comparison.json').read_text())['matched_cases']
            self.assertEqual(len(diff),1)
            self.assertEqual(diff[0]['near_wake_mean_vx_change_m_s'],0)
            self.assertEqual(diff[0]['poisson_residual_change_s_inv'],0)
            self.assertEqual(len(list(b.glob('*/near_wake.png'))),1)
            self.assertIn('not qualified',(b/'report.md').read_text())
            report=json.loads(next(b.glob('*/report.json')).read_text())
            self.assertEqual(report['steady_screen']['status'],'insufficient_evidence')
            self.assertEqual(len(report['wake_series']),1)
            self.assertEqual(report['wake_series'][0]['tick'],report['snapshot']['tick'])

    def test_prescribed_inlet_and_solver_budget(self):
        with tempfile.TemporaryDirectory(prefix='s3-inlet-') as tmp:
            s=Service(tmp); scene=s.scene_create('empty',template='wind_empty',inflow_speed=2)
            rev=scene['scene_revision']
            settings=dict(grid=[32,16,16],fluid={'density_kg_m3':1.1612,'dynamic_viscosity_pa_s':1.85e-5},
                          qualification_mode=True,solver_iterations=128)
            with self.assertRaises(SessionError):
                s.scene_validate('empty',rev,**dict(settings,solver_cell_budget=100))
            with self.assertRaises(SessionError):
                s.scene_validate('empty',rev,**dict(settings,qualification_mode=False))
            with self.assertRaises(SessionError):
                s.scene_validate('empty',rev,**dict(settings,solver_iterations=513))
            s.run_start('inlet','empty',rev,steps=3,**settings)
            try:
                self.assertEqual(s.run_control('inlet','one','step',rev,5000)['status'],'applied')
                snap=s.run_inspect('inlet')
                self.assertEqual(snap['health']['solved_clusters'],1)
                self.assertEqual(snap['health']['projection_status'],'converged')
                self.assertEqual(snap['health']['velocity_transport'],'bounded_maccormack_visible_donors_v1')
                self.assertGreater(snap['health']['transport_corrected_components'],0)
                self.assertGreaterEqual(snap['health']['transport_fallback_components'],0)
                self.assertLessEqual(snap['health']['transport_limited_components'],snap['health']['transport_corrected_components'])
                self.assertLessEqual(snap['health']['projection_iterations_used'],128)
                sample=s.run_sample('inlet','port',plane='YZ',position=0,resolution=32,field='vx',wait_ms=5000)
                fluid=[c[3] for c in sample['preview']['samples'] if c[2]<.5]
                self.assertTrue(fluid)
                self.assertTrue(all(v==2 for v in fluid))
            finally:
                s.run_control('inlet','finish','cancel',rev,5000)
                s.children['inlet'].wait(timeout=10)

    def test_wake_aperture_separates_transport_from_deficit(self):
        import importlib.util
        spec=importlib.util.spec_from_file_location('qualify_fluid',ROOT/'scripts/qualify_fluid.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        samples=[[0,0,0,2] for _ in range(16)]
        for k in (5,6,9,10):samples[k]=[0,0,0,0]
        p={'u_axis':1,'v_axis':2,'world_bounds_m':[0,0,0,1,1,1],
           'grid':[4,4,4],'width':4,'height':4,'samples':samples}
        profile=module.wake_profile(p,2,.5)
        self.assertEqual(profile['mean_vx_m_s'],1.5)
        self.assertEqual(profile['central_wake']['sample_count'],4)
        self.assertEqual(profile['central_wake']['normalized_mean_deficit'],1)
        samples[5]=[0,0,1,100]
        self.assertEqual(module.wake_profile(p,2,.5)['central_wake']['sample_count'],3)

    def test_steady_screen_rejects_drift_oscillation_and_missing_health(self):
        def fixture(fn):
            series=[]; history=[]
            for i in range(1,61):
                t=i/10
                station={'central_mean_vx_m_s':fn(t),'section_mean_vx_m_s':2,'reverse_flow_fraction':0}
                series.append({'time_s':t,'near_wake':station,'far_wake':station})
                history.append({'time_s':t,'health':{'projection_status':'converged',
                    'conservation':{'available':True,'outward_face_flux_m3_s_minmax_xyz':[-2,2,0,0,0,0],
                                    'net_outward_volume_flux_m3_s':0}}})
            return series,history
        series,history=fixture(lambda t:1)
        self.assertEqual(steady_screen(series,history,2,2)['status'],'steady_window_screen_pass')
        for fn,reason in [(lambda t:t,'wake_mean_drift'),(lambda t:math.cos(2*math.pi*t),'wake_fluctuation')]:
            a,b=fixture(fn); self.assertIn(reason,steady_screen(a,b,2,2)['reasons'])
        self.assertIn('missing_solver_history',steady_screen(series,history[-1:],2,2)['reasons'])
        history[-2]['health']['projection_status']='not_converged'
        self.assertIn('pressure_not_converged',steady_screen(series,history,2,2)['reasons'])
        self.assertEqual(steady_screen(series[:10],history[:10],2,2)['status'],'insufficient_evidence')
        series[-1]['near_wake']['central_mean_vx_m_s']=None
        self.assertIn('nonfinite_or_empty_wake_aperture',steady_screen(series,history,2,2)['reasons'])

    def test_refinement_requires_matched_settled_levels(self):
        def case(h,value):
            return {'shape':'sphere','fluid':{'rho':1},'duration_s':6,'dt_s':.02,
                'validation':{'voxel_size_m':h},'reference':{'characteristic_length_m':.3},
                'snapshot':{'health':{'projection_operator':'test'}},
                'request':{'worker_sha256':'same','geometry_assets':{},'qualification_mode':True,'solver_iterations':256},
                'study_setup':{'dimensions_m':[2,1,1],'object_center_m':[.9,.5,.5],'speed_m_s':2,
                    'wake_positions_m':[1.2,1.8],'sampling_interval_s':.1},
                'steady_screen':{'status':'steady_window_screen_pass','criteria':{},'metrics':{
                    station+'.central_mean_vx_m_s':{'window_means':[value,value]} for station in ('near_wake','far_wake')}}}
        cases=[case(.1,1),case(.05,1.04),case(.025,1.05)]
        self.assertEqual(refinement_screen(cases,'spatial')['status'],'numerical_sensitivity_screen_pass')
        self.assertFalse(refinement_screen(cases,'spatial')['force_measurement_ready'])
        self.assertIn('need_3_independent_levels',refinement_screen(cases[:2],'spatial')['reasons'])
        cases[2]['request']['worker_sha256']='different'
        self.assertIn('unmatched_controls_or_worker',refinement_screen(cases,'spatial')['reasons'])
        cases[2]['request']['worker_sha256']='same'
        cases[2]['steady_screen']['status']='not_steady_or_numerically_unresolved'
        self.assertIn('unsettled_or_unresolved_case',refinement_screen(cases,'spatial')['reasons'])
        cases[2]['steady_screen']['status']='steady_window_screen_pass'
        cases[2]['steady_screen']['metrics']['near_wake.central_mean_vx_m_s']['window_means']=[1,float('nan')]
        self.assertNotEqual(refinement_screen(cases,'spatial')['status'],'numerical_sensitivity_screen_pass')
        pair=[case(.05,1),case(.05,1.01)]
        pair[1]['study_setup']['dimensions_m'][0]=3
        self.assertEqual(refinement_screen(pair,'outlet')['status'],'numerical_sensitivity_screen_pass')
        pair[1]['study_setup']['object_center_m'][0]=1.35
        self.assertIn('unmatched_controls_or_worker',refinement_screen(pair,'outlet')['reasons'])

    def test_fixed_object_position_for_outlet_study(self):
        with tempfile.TemporaryDirectory() as tmp:
            s=Service(tmp)
            a=call(s,'scene_create',dict(scene_id='long',template='wind_stl_sphere',dimensions=[3,1,1],
                object_size_m=.3,object_center_m=[.9,.5,.5]))
            doc=json.loads((Path(tmp)/'scenes/long/scene_authoring.json').read_text())
            self.assertEqual(doc['objects'][0]['transform']['position'],{'x':.9,'y':.5,'z':.5})
            with self.assertRaises(SessionError):
                s.scene_create('bad',object_center_m=[0,.5,.5],object_size_m=.3)

    def test_invalid_fluid_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            s=Service(tmp);scene=s.scene_create('test');rev=scene['scene_revision']
            for fluid in ({'density_kg_m3':1,'dynamic_viscosity_pa_s':0},
                          {'density_kg_m3':float('nan'),'dynamic_viscosity_pa_s':1},
                          {'density_kg_m3':1,'dynamic_viscosity_pa_s':True},{}):
                with self.assertRaises(SessionError):s.scene_validate('test',rev,fluid=fluid)
            with self.assertRaises(SessionError):s.scene_validate('test',rev,qualification_mode=True)
            with self.assertRaisesRegex(SessionError,'1024'):s.scene_validate('test',rev,fluid={'density_kg_m3':.001,'dynamic_viscosity_pa_s':1000})

if __name__=='__main__':unittest.main(verbosity=2)
