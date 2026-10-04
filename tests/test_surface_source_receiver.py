"""Cross-program source validation, conserved planning, replay and rollback."""
import concurrent.futures
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import sqlite3
from contextlib import closing
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from surface_sources.receiver import Receiver,allocate,policy_valid
from surface_sources.growth_fire_v1 import sealed,strict_load,digest

FIXTURES=ROOT/'tests/fixtures/surface_sources'


def policy(frame):
    return {'schema':'physics_sim_surface_source_policy/v1','consumer_id':'atmosphere-patch',
        'model_id':'fluid_atmosphere_offline_allocation_v1','source_config':copy.deepcopy(frame['config']),
        'producer':copy.deepcopy(frame['producer']),'stream_start_tick':0,
        'receiver':{'frame':'right_handed_z_up_meters','origin_m':[0,0,0],
                    'spacing_m':[.1,.1,.1],'dimensions':[7,7,2],'layer':0,'fluid_mask':[1]*49}}


class SourceTests(unittest.TestCase):
    def setUp(self):
        self.frame=strict_load(FIXTURES/'prescribed-patch.json')
        self.policy=policy(self.frame);self.temp=tempfile.TemporaryDirectory()
        self.receiver=Receiver(self.temp.name,self.policy)

    def tearDown(self):self.temp.cleanup()

    def test_prescribed_temporal_and_spatial_known_answer(self):
        self.receiver.admit(self.frame)
        p=self.receiver.plan(0,[0,.005,.02,.05],'prescribed')['receipt']
        self.assertEqual([s['totals']['energy_transferred_j'] for s in p['steps']],[600,1800,3600])
        for i,expected in enumerate((.00008,.00024,.00048)):
            self.assertAlmostEqual(p['steps'][i]['totals']['smoke_transferred_kg'],expected,places=16)
        # Interior node 9 at (.1,.1) touches four receiver cells equally.
        cells=self.receiver.plan(0,[0,.05],'spatial')['receipt']['steps'][0]['cells']
        self.assertEqual([c['cell_index'] for c in cells],[0,1,7,8])
        for c in cells:self.assertAlmostEqual(c['energy_transferred_j'],1500,places=10)
        self.assertEqual(sum(c['energy_transferred_j'] for c in cells),6000)
        self.assertFalse(p['physically_applied']);self.assertFalse(p['solver_mutated'])

    def test_partial_interval_retains_budget(self):
        p=allocate(self.frame,self.policy,[.005,.02],'partial')
        b=p['budgets']['energy_transferred_j']
        self.assertAlmostEqual(b['retained_before'],600);self.assertAlmostEqual(b['planned'],1800)
        self.assertAlmostEqual(b['remaining_after'],3600)
        self.assertAlmostEqual(sum(b[k] for k in ('retained_before','planned','remaining_after')),b['source'])

    def test_native_bundle_checkpoint_validation(self):
        bundle=strict_load(FIXTURES/'native-bundle.json');p=policy(bundle['frame'])
        p['receiver'].update(spacing_m=[.125,.125,.125],dimensions=[16,16,16],layer=2,fluid_mask=[1]*256)
        r=Receiver(self.temp.name,p)
        changed=copy.deepcopy(bundle);changed['checkpoint']['native_state_sha256']='0'*64
        with self.assertRaises(ValueError):r.admit(sealed(changed))
        self.assertFalse(r.path.exists())
        r.admit(bundle);before=r.path.read_bytes()
        self.assertEqual(r.admit(bundle['frame'])['status'],'replayed')
        self.assertEqual(before,r.path.read_bytes())

    def test_zero_source(self):
        frame=strict_load(FIXTURES/'zero-source.json');r=Receiver(self.temp.name,policy(frame));r.admit(frame)
        p=r.plan(0,[0,.05],'zero')['receipt'];self.assertEqual(p['steps'][0]['cells'],[])
        self.assertTrue(all(v['planned']==0 for v in p['budgets'].values()))

    def test_event_replay_restart_and_content_conflict(self):
        first=self.receiver.admit(self.frame);before=self.receiver.path.read_bytes()
        r=Receiver(self.temp.name,self.policy);replayed=r.admit(copy.deepcopy(self.frame))
        self.assertEqual(replayed['status'],'replayed');self.assertEqual(replayed['receipt'],first['receipt'])
        self.assertEqual(before,r.path.read_bytes())
        changed=copy.deepcopy(self.frame);changed['diagnostics']['legacy_heat_generated_units']=1
        with self.assertRaises(ValueError):r.admit(sealed(changed))
        self.assertEqual(before,r.path.read_bytes())

    def test_successor_order_clock_and_policy_binding(self):
        self.receiver.admit(self.frame)
        next_frame=copy.deepcopy(self.frame);next_frame['event']['sequence']=1
        next_frame['interval'].update(start_tick=3,end_tick=6,start_s=.05,end_s=.1)
        self.receiver.admit(sealed(next_frame));self.assertEqual(self.receiver.inspect()['next_tick'],6)
        before=self.receiver.path.read_bytes()
        skipped=copy.deepcopy(next_frame);skipped['event']['sequence']=3
        with self.assertRaises(ValueError):self.receiver.admit(sealed(skipped))
        changed=copy.deepcopy(self.policy);changed['consumer_id']='other'
        with self.assertRaises(ValueError):Receiver(self.temp.name,changed).admit(self.frame)
        self.assertEqual(before,self.receiver.path.read_bytes())

    def test_rejections_do_not_create_journal(self):
        mutations=[lambda f:f['event'].__setitem__('sequence',1),
            lambda f:f['producer'].__setitem__('worker_sha256','1'*64),
            lambda f:f['cells']['energy_transferred_j'].__setitem__(9,-1),
            lambda f:f['surface']['normal'].__setitem__(2,-1),
            lambda f:f['config']['calibration'].__setitem__('energy_transfer_fraction',.5),
            lambda f:f['config'].__setitem__('width',True),
            lambda f:f['config']['origin_m'].__setitem__(0,-0.0)]
        for mutate in mutations:
            f=copy.deepcopy(self.frame);mutate(f)
            with self.assertRaises(ValueError):self.receiver.admit(sealed(f))
            self.assertFalse(self.receiver.path.exists())
        f=copy.deepcopy(self.frame);f['totals']['energy_generated_j']=10001
        with self.assertRaises(ValueError):self.receiver.admit(f)

    def test_geometry_and_receiver_mask_rejections(self):
        for mutate in (lambda p:p['receiver']['origin_m'].__setitem__(2,.1),
                       lambda p:p['receiver']['fluid_mask'].__setitem__(0,0),
                       lambda p:p['receiver']['dimensions'].__setitem__(0,6)):
            p=copy.deepcopy(self.policy);mutate(p)
            with self.assertRaises(ValueError):Receiver(self.temp.name,p).admit(self.frame)
            self.assertFalse(self.receiver.path.exists())

    def test_plan_replay_conflict_and_invalid_substeps(self):
        self.receiver.admit(self.frame);first=self.receiver.plan(0,[0,.05],'one');before=self.receiver.path.read_bytes()
        self.assertEqual(self.receiver.plan(0,[0,.05],'one')['receipt'],first['receipt'])
        for boundaries in ([0,.02,.05],[0,0],[0,.06],[True,.05],[.02,.01],[float('nan'),.05]):
            with self.assertRaises(ValueError):self.receiver.plan(0,boundaries,'one')
            self.assertEqual(before,self.receiver.path.read_bytes())
        with self.assertRaises(ValueError):self.receiver.plan(1,[0,.05],'unknown')

    def test_concurrent_equal_admission(self):
        def admit(_):return Receiver(self.temp.name,self.policy).admit(self.frame)['status']
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            result=list(pool.map(admit,range(4)))
        self.assertEqual(result.count('admitted'),1);self.assertEqual(result.count('replayed'),3)
        self.assertEqual(self.receiver.inspect()['admitted_events'],1)

    def test_duplicate_keys_nonfinite_and_cli(self):
        root=Path(self.temp.name);p=root/'policy.json';p.write_text(json.dumps(self.policy))
        for text in ('{"x":1,"x":2}','{"x":NaN}','{"x":Infinity}'):
            bad=root/'bad.json';bad.write_text(text)
            with self.assertRaises(ValueError):strict_load(bad)
        boundaries=root/'boundaries.json';boundaries.write_text('[0,.005,.02,.05]'.replace('.005','0.005').replace('.02','0.02').replace('.05','0.05'))
        command=[sys.executable,'-B',str(ROOT/'scripts/physics_sim_surface_source.py'),
            '--store',str(root/'cli-store'),'--policy',str(p)]
        a=subprocess.run(command+['admit',str(FIXTURES/'prescribed-patch.json')],capture_output=True,text=True)
        self.assertEqual(a.returncode,0,a.stderr);self.assertEqual(json.loads(a.stdout)['status'],'admitted')
        a=subprocess.run(command+['allocate','--sequence','0','--plan-id','cli','--boundaries',str(boundaries)],capture_output=True,text=True)
        self.assertEqual(a.returncode,0,a.stderr);self.assertEqual(json.loads(a.stdout)['receipt']['budgets']['energy_transferred_j']['planned'],6000)

    def test_unequal_receiver_cells_conserve_area_budget(self):
        p=copy.deepcopy(self.policy);p['receiver'].update(spacing_m=[.08,.1,.1],dimensions=[9,7,2],fluid_mask=[1]*63)
        plan=allocate(self.frame,p,[0,.05],'unequal')
        cells=plan['steps'][0]['cells']
        self.assertEqual([c['cell_index'] for c in cells],[0,1,9,10])
        for cell,q in zip(cells,[900,2100,900,2100]):self.assertAlmostEqual(cell['energy_transferred_j'],q,places=10)
        self.assertAlmostEqual(plan['budgets']['energy_transferred_j']['planned'],6000)

    def test_capacity_rejection_and_sql_transaction_rollback(self):
        self.receiver.admit(self.frame);next_frame=copy.deepcopy(self.frame)
        next_frame['event']['sequence']=1;next_frame['interval'].update(start_tick=3,end_tick=6,start_s=.05,end_s=.1)
        next_frame=sealed(next_frame);before=self.receiver.path.read_bytes()
        with patch('surface_sources.receiver.MAX_EVENTS',1):
            with self.assertRaises(ValueError):self.receiver.admit(next_frame)
        self.assertEqual(before,self.receiver.path.read_bytes())
        with closing(sqlite3.connect(self.receiver.path)) as db:
            db.execute("CREATE TRIGGER fail_event BEFORE INSERT ON events BEGIN SELECT RAISE(ABORT,'test failure'); END")
        before=self.receiver.path.read_bytes()
        with self.assertRaises(sqlite3.IntegrityError):self.receiver.admit(next_frame)
        self.assertEqual(before,self.receiver.path.read_bytes())
        self.assertEqual(self.receiver.inspect()['admitted_events'],1)
        self.assertEqual(self.receiver.inspect()['next_tick'],3)

    @unittest.skipUnless(os.environ.get('PHYSICS_SIM_SESSION_WORKER'), 'selected source worker for unchanged-solver proof')
    def test_paused_and_completed_solver_unchanged(self):
        sys.path.insert(0,str(ROOT/'scripts/agent_session'))
        from service import Service
        from test_agent_refined import wait
        with tempfile.TemporaryDirectory() as root:
            service=Service(root);scene=service.scene_create('flow','cfd_manufactured_3d',dimensions=[2,2.5,3])
            service.run_start('flow',scene_id='flow',scene_revision=scene['scene_revision'],grid=[8,8,8],steps=2,dt=.01,
                fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=16)
            try:
                self.assertEqual(wait(service,'flow')['state'],'paused')
                before={k:v for k,v in service.run_inspect('flow').items() if k!='snapshot_age_seconds'}
                self.receiver.admit(self.frame);self.receiver.plan(0,[0,.005,.02,.05],'paused-proof')
                self.assertEqual({k:v for k,v in service.run_inspect('flow').items() if k!='snapshot_age_seconds'},before)
                service.run_control('flow','complete','continue',scene['scene_revision'],5000)
                self.assertEqual(wait(service,'flow',True)['state'],'completed')
                before={k:v for k,v in service.run_inspect('flow').items() if k!='snapshot_age_seconds'};result=service.run_result('flow')
                files={a['path']:hashlib.sha256(Path(a['path']).read_bytes()).hexdigest() for a in result['artifacts']}
                self.receiver.admit(self.frame);self.receiver.plan(0,[.005,.02],'completed-proof')
                self.assertEqual({k:v for k,v in service.run_inspect('flow').items() if k!='snapshot_age_seconds'},before)
                self.assertEqual({p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in files},files)
                if os.environ.get('PHYSICS_SIM_SURFACE_PROOF'):
                    proof={'status':'passed','paused_status_unchanged':True,'completed_status_unchanged':True,
                        'snapshot_age_excluded':True,'solver_mutated':False,'physically_applied':False,
                        'worker_sha256':hashlib.sha256(Path(os.environ['PHYSICS_SIM_SESSION_WORKER']).read_bytes()).hexdigest(),
                        'artifacts_sha256':{Path(p).name:h for p,h in files.items()}}
                    target=Path(os.environ['PHYSICS_SIM_SURFACE_PROOF']);target.parent.mkdir(parents=True,exist_ok=True)
                    target.write_text(json.dumps(proof,sort_keys=True,indent=2)+'\n')
            finally:
                process=service.children.get('flow')
                if process and process.poll() is None:
                    service.run_control('flow','cleanup','cancel',scene['scene_revision'],5000)
                    process.wait(timeout=10)

    def test_vendor_identity_and_numeric_digest(self):
        manifest=json.loads((ROOT/'scripts/surface_sources/upstream.json').read_text())
        self.assertEqual(hashlib.sha256((ROOT/'scripts/surface_sources/growth_fire_v1.py').read_bytes()).hexdigest(),manifest['sha256'])
        expected=hashlib.sha256(b'{"value":{"$f64be":"3ff0000000000000"}}').hexdigest()
        self.assertEqual(digest({'value':1}),expected);self.assertEqual(digest({'value':1.0}),expected)


if __name__=='__main__':unittest.main()
