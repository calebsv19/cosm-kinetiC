import copy
import shutil
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupled_passive import Coupled
from surface_sources.growth_fire_v1 import strict_load,sealed
class CoupledTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.frame=strict_load(ROOT/'tests/fixtures/surface_sources/prescribed-patch.json')
        self.policy=strict_load(ROOT/'tests/fixtures/surface_sources/prescribed-policy.json');self.policy['receiver']['dimensions'][2]=4
        self.props={'density_kg_m3':1,'heat_capacity_j_kg_k':1000,'reference_temperature_k':300,'conductivity_w_m_k':0,'tracer_diffusivity_m2_s':0}
        self.worker=ROOT/'build/passive-atmosphere/physics_sim_passive_worker'
        self.receiver=self.reopen();self.receiver.initialize()
    def tearDown(self):self.temp.cleanup()
    def reopen(self):return Coupled(self.temp.name,self.policy,self.props,[0,0,0],self.worker)
    def test_partial_restart_replay_and_full_consumption(self):
        self.receiver.admit(self.frame);first=self.receiver.step(0,'first',.02);saved=self.receiver.path.read_bytes()
        self.assertEqual(self.reopen().step(0,'first',.02),first);self.assertEqual(saved,self.receiver.path.read_bytes())
        self.assertAlmostEqual(self.reopen().inspect()['result']['budgets']['energy_j']['stored'],2400)
        self.reopen().step(1,'remaining',.03);cp=self.reopen().inspect()
        self.assertAlmostEqual(cp['result']['budgets']['energy_j']['stored'],6000)
        self.assertEqual(cp['consumed'][-1]['end_s'],.05)
        self.assertEqual(self.receiver.admit(self.frame)['status'],'admitted_not_consumed')
        self.reopen().step(2,'transport',.01,'none');self.assertEqual(self.reopen().inspect()['result']['budgets']['energy_j']['stored'],6000)
    def test_failure_and_stale_revision_preserve_everything(self):
        self.receiver.admit(self.frame);before=self.receiver.path.read_bytes()
        with patch('coupled_passive.run',side_effect=RuntimeError('cancelled worker')):
            with self.assertRaises(RuntimeError):self.receiver.step(0,'cancelled',.02)
        self.assertEqual(before,self.receiver.path.read_bytes());self.assertEqual(self.receiver.inspect()['revision'],0)
        with self.assertRaises(ValueError):self.receiver.step(0,'skip',.02,'none')
        with self.assertRaises(ValueError):self.receiver.step(0,'gap',.1)
        self.assertEqual(before,self.receiver.path.read_bytes())
        self.receiver.step(0,'first',.02);before=self.receiver.path.read_bytes()
        with self.assertRaises(ValueError):self.receiver.step(0,'stale',.01)
        with self.assertRaises(ValueError):self.receiver.step(0,'first',.01)
        self.assertEqual(before,self.receiver.path.read_bytes())
    def test_source_conflict_order_binding_and_sql_rollback(self):
        self.receiver.admit(self.frame);before=self.receiver.path.read_bytes()
        changed=copy.deepcopy(self.frame);changed['diagnostics']['legacy_heat_generated_units']=123
        with self.assertRaises(ValueError):self.receiver.admit(sealed(changed))
        self.assertEqual(before,self.receiver.path.read_bytes())
        with closing(sqlite3.connect(self.receiver.path)) as db:
            db.execute("CREATE TRIGGER fail_cp BEFORE INSERT ON checkpoints BEGIN SELECT RAISE(ABORT,'test'); END")
        before=self.receiver.path.read_bytes()
        with self.assertRaises(sqlite3.IntegrityError):self.receiver.step(0,'failed-publish',.01)
        self.assertEqual(before,self.receiver.path.read_bytes());self.assertEqual(self.receiver.inspect()['revision'],0)
    def test_database_copy_binding_and_cross_event_remainder(self):
        self.receiver.admit(self.frame);second=copy.deepcopy(self.frame)
        second['event']['sequence']=1;second['interval'].update(start_tick=3,end_tick=6,start_s=.05,end_s=.1)
        self.receiver.admit(sealed(second));self.receiver.step(0,'cross-event',.08)
        self.assertAlmostEqual(self.receiver.inspect()['result']['budgets']['energy_j']['stored'],9600)
        with tempfile.TemporaryDirectory() as cloned:
            shutil.copyfile(self.receiver.path,Path(cloned)/'coupled.sqlite3')
            restored=Coupled(cloned,self.policy,self.props,[0,0,0],self.worker)
            restored.step(1,'finish',.02);self.assertAlmostEqual(restored.inspect()['result']['budgets']['energy_j']['stored'],12000)
        before=self.receiver.path.read_bytes();changed=dict(self.props,density_kg_m3=2)
        with self.assertRaises(ValueError):Coupled(self.temp.name,self.policy,changed,[0,0,0],self.worker).inspect()
        self.assertEqual(before,self.receiver.path.read_bytes())

    def test_split_restart_exact_uninterrupted_step_history(self):
        self.receiver.admit(self.frame);self.receiver.step(0,'one',.02);self.reopen().step(1,'two',.03)
        final=self.reopen().inspect()
        from passive_atmosphere import run
        self.assertEqual(final['result'],run(final['request'],self.worker))
if __name__=='__main__':unittest.main()
