import copy
import math
from contextlib import closing
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupled_open_atmosphere import CoupledOpenAtmosphere
from open_atmosphere import run,SCHEMA
from surface_sources.growth_fire_v1 import strict_load,sealed
class JournalTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.frame=strict_load(ROOT/'tests/fixtures/surface_sources/prescribed-patch.json')
        self.policy=strict_load(ROOT/'tests/fixtures/surface_sources/prescribed-policy.json');self.policy['receiver']['dimensions'][2]=4
        self.props={'density_kg_m3':1,'dynamic_viscosity_pa_s':.02,'heat_capacity_j_kg_k':1000,'reference_temperature_k':300,'conductivity_w_m_k':.1,'tracer_diffusivity_m2_s':.001}
        nx,ny,nz=self.policy['receiver']['dimensions'];n=nx*ny*nz
        self.velocity=[.2+.05*math.cos(2*math.pi*((q//nx)%ny+.5)/ny) for q in range(n)]+[.05]*n+[.1]*(n+nx*ny)
        self.boundary={'schema':'physics_sim_open_reservoir_boundary/v1','horizontal':'periodic_xy','vertical':'open_bottom_and_top','predictor_velocity':'zero_normal_gradient','pressure_datum_pa':[0,0],'inflow_temperature_k':[300,300],'inflow_smoke_concentration_kg_m3':[0,0],'scalar_diffusion':'zero_normal_flux'}
        self.buoyancy={'enabled':False,'gravity_m_s2':9.81,'expansion_per_k':1/300,'max_temperature_contrast_fraction':.1}
        self.worker=ROOT/'build/open-atmosphere/physics_sim_open_atmosphere_worker';self.receiver=self.reopen();self.receiver.initialize();self.receiver.admit(self.frame)
    def tearDown(self):self.temp.cleanup()
    def reopen(self):return CoupledOpenAtmosphere(self.temp.name,self.policy,self.props,self.velocity,.01,self.boundary,self.buoyancy,self.worker)
    def test_explicit_journal_budget_binding_and_bounds(self):
        with tempfile.TemporaryDirectory() as root:
            def receiver(budget):return CoupledOpenAtmosphere(root,self.policy,self.props,self.velocity,.01,self.boundary,self.buoyancy,self.worker,budget)
            receiver(256*1024*1024).initialize()
            self.assertEqual(receiver(256*1024*1024).inspect()['revision'],0)
            with self.assertRaises(ValueError):receiver(128*1024*1024).initialize()
            for bad in (True,0,513*1024*1024):
                with self.assertRaises(ValueError):receiver(bad)

    def test_atomic_batch_retry_and_late_failure(self):
        second=copy.deepcopy(self.frame);second['event']['sequence']=1
        second['interval'].update(start_tick=3,end_tick=6,start_s=.05,end_s=.1)
        second=sealed(second)
        third=copy.deepcopy(second);third['event']['sequence']=3
        third['interval'].update(start_tick=6,end_tick=9,start_s=.1,end_s=.15)
        before=self.receiver.path.read_bytes()
        with self.assertRaises(ValueError):self.receiver.admit_many([second,sealed(third)])
        self.assertEqual(before,self.receiver.path.read_bytes())
        received=self.receiver.admit_many([self.frame,second])
        self.assertEqual(received[0],self.receiver.admit(self.frame))
        before=self.receiver.path.read_bytes()
        self.assertEqual(received,self.reopen().admit_many([self.frame,second]))
        self.assertEqual(before,self.receiver.path.read_bytes())
        conflicting=copy.deepcopy(second);conflicting['diagnostics']['legacy_heat_generated_units']=123
        with self.assertRaises(ValueError):self.receiver.admit_many([self.frame,sealed(conflicting)])
        self.assertEqual(before,self.receiver.path.read_bytes())
        for bad in ([],[self.frame]*257):
            with self.assertRaises(ValueError):self.receiver.admit_many(bad)
        self.receiver.step(0,'first-batch-half',.05)
        self.receiver.step(1,'second-batch-half',.05)
        self.assertAlmostEqual(self.receiver.inspect()['result']['budgets']['energy_j']['input'],12000)

    def test_restart_retry_consumption_and_native_increment(self):
        first=self.receiver.step(0,'first',.02);before=self.receiver.path.read_bytes()
        self.assertEqual(first,self.reopen().step(0,'first',.02));self.assertEqual(before,self.receiver.path.read_bytes())
        with patch('coupled_open_atmosphere.run',wraps=run) as native:
            self.reopen().step(1,'finish',.03)
            request=native.call_args[0][0];self.assertEqual(len(request['steps']),3);self.assertEqual(request['state']['data']['steps'],2)
        cp=self.reopen().inspect();self.assertEqual(cp['result']['state']['data']['steps'],5)
        self.assertAlmostEqual(cp['result']['budgets']['energy_j']['stored']+cp['result']['budgets']['energy_j']['outflow']-cp['result']['budgets']['energy_j']['inflow'],6000)
        self.assertNotEqual(self.velocity,cp['result']['fields']['face_velocity_m_s'])
        self.reopen().step(2,'transport',.02,'none');budget=self.reopen().inspect()['result']['budgets']['energy_j'];self.assertAlmostEqual(budget['stored']+budget['outflow']-budget['inflow'],6000)
    def test_failed_worker_publish_gap_and_fixed_dt_preserve_database(self):
        before=self.receiver.path.read_bytes()
        for duration,mode in ((.06,'required'),(.02,'none'),(.015,'required')):
            with self.assertRaises(ValueError):self.receiver.step(0,'invalid',duration,mode)
            self.assertEqual(before,self.receiver.path.read_bytes())
        with patch('coupled_open_atmosphere.run',side_effect=RuntimeError('worker failure')):
            with self.assertRaises(RuntimeError):self.receiver.step(0,'failed',.02)
        self.assertEqual(before,self.receiver.path.read_bytes())
        with closing(sqlite3.connect(self.receiver.path)) as db:db.execute("CREATE TRIGGER fail BEFORE INSERT ON checkpoints BEGIN SELECT RAISE(ABORT,'test'); END")
        before=self.receiver.path.read_bytes()
        with self.assertRaises(sqlite3.IntegrityError):self.receiver.step(0,'publication-failure',.02)
        self.assertEqual(before,self.receiver.path.read_bytes());self.assertEqual(self.receiver.inspect()['revision'],0)
    def test_contrast_failure_preserves_source_and_native_history(self):
        with tempfile.TemporaryDirectory() as root:
            receiver=CoupledOpenAtmosphere(root,self.policy,self.props,self.velocity,.01,self.boundary,dict(self.buoyancy,enabled=True),self.worker)
            receiver.initialize();receiver.admit(self.frame);before=receiver.path.read_bytes()
            with self.assertRaises(ValueError):receiver.step(0,'hot-outside-envelope',.05)
            self.assertEqual(before,receiver.path.read_bytes());self.assertEqual(receiver.inspect()['revision'],0)
    def test_next_admission_uses_integer_step_clock(self):
        for i in range(1,4):
            frame=copy.deepcopy(self.frame);frame['event']['sequence']=i
            frame['interval'].update(start_tick=3*i,end_tick=3*(i+1),start_s=3*i*self.frame['config']['dt_s'],end_s=3*(i+1)*self.frame['config']['dt_s'])
            self.receiver.admit(sealed(frame))
        self.receiver.step(0,'twenty-native-steps',.2)
        next_frame=copy.deepcopy(self.frame);next_frame['event']['sequence']=4
        next_frame['interval'].update(start_tick=12,end_tick=15,start_s=12*self.frame['config']['dt_s'],end_s=15*self.frame['config']['dt_s'])
        self.assertEqual(self.receiver.admit(sealed(next_frame))['status'],'admitted_not_consumed')
    def test_source_boundary_inside_fixed_momentum_step(self):
        second=copy.deepcopy(self.frame);second['event']['sequence']=1
        second['interval'].update(start_tick=3,end_tick=6,start_s=.05,end_s=.1)
        with tempfile.TemporaryDirectory() as root:
            receiver=CoupledOpenAtmosphere(root,self.policy,self.props,self.velocity,.02,self.boundary,self.buoyancy,self.worker)
            receiver.initialize();receiver.admit(self.frame);receiver.admit(sealed(second));receiver.step(0,'cross-boundary',.1)
            cp=receiver.inspect();self.assertEqual(cp['result']['state']['data']['steps'],5)
            self.assertAlmostEqual(cp['result']['budgets']['energy_j']['stored']+cp['result']['budgets']['energy_j']['outflow']-cp['result']['budgets']['energy_j']['inflow'],12000)
            ranges=cp['consumed'];boundary=next(i for i,c in enumerate(ranges) if c['end_s']==.05)
            self.assertEqual(ranges[boundary+1]['start_s'],.05)
            self.assertEqual(ranges[boundary+1]['end_s'],.06)
    def test_copied_native_state_source_conflict_and_revision(self):
        self.receiver.step(0,'partial',.02);before=self.receiver.path.read_bytes()
        with self.assertRaises(ValueError):self.receiver.step(0,'stale',.01)
        with self.assertRaises(ValueError):self.receiver.step(0,'partial',.01)
        changed=copy.deepcopy(self.frame);changed['diagnostics']['legacy_heat_generated_units']=123
        with self.assertRaises(ValueError):self.receiver.admit(sealed(changed))
        self.assertEqual(before,self.receiver.path.read_bytes())
        with tempfile.TemporaryDirectory() as clone:
            shutil.copyfile(self.receiver.path,Path(clone)/'coupled.sqlite3')
            restored=CoupledOpenAtmosphere(clone,self.policy,self.props,self.velocity,.01,self.boundary,self.buoyancy,self.worker);restored.step(1,'finish',.03)
            self.receiver.step(1,'finish',.03);self.assertEqual(restored.inspect(),self.receiver.inspect())
if __name__=='__main__':unittest.main()
