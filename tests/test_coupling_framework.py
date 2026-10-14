"""Real control-plane refusal, identity, and native joint-publication contracts."""
import copy
from contextlib import closing
import json
import os
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.framework import Framework
from coupling.scenario import baseline
from surface_sources.growth_fire_v1 import canonical,digest

class FrameworkTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='fire-fluid-contract-');self.root=Path(self.temp.name)
        growth=Path(os.environ['FIRE_FLUID_GROWTH_ROOT'])
        self.config=baseline(growth,os.environ['FIRE_FLUID_FIRE_WORKER'],os.environ['FIRE_FLUID_FLUID_WORKER'],os.environ['FIRE_FLUID_CLOCK_WORKER'])
        self.prior=os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT');os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(self.root/'evidence')
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(self.restore)
    def restore(self):
        if self.prior is None:os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT',None)
        else:os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=self.prior
    def test_schedule_fractions_and_unaligned_refusal_before_solver(self):
        host=Framework(self.config,self.root/'store')
        plan=host.clock(dict(host.base_plan,initial_id=1,initial_validation=1,initial_checkpoints=[1,2],history=[]))
        self.assertEqual(plan['rate'],200);self.assertEqual(plan['schedule'][0]['fire_steps'],2)
        steps=plan['schedule'][0]['steps'];self.assertEqual(len(steps),8)
        self.assertEqual((steps[-1]['right_n'],steps[-1]['right_d']),(1,1))
        self.config['periods'][2]=[1,20]
        with self.assertRaisesRegex(ValueError,'schedule'):Framework(self.config,self.root/'bad')
        self.assertFalse((self.root/'bad').exists())
    def test_native_joint_rejects_stale_predecessor(self):
        host=Framework(self.config,self.root/'store');initial=host.initialize();host.step(0,'first')
        with closing(sqlite3.connect(host.path)) as db, db:rows=[json.loads(v[0]) for v in db.execute('SELECT payload FROM checkpoints ORDER BY revision')]
        request=dict(host.base_plan,initial_id=initial['id'],initial_validation=int(digest(initial['evidence'])[:13],16),
            initial_checkpoints=[int(digest(initial['source'])[:13],16),int(digest(initial['fluid']['state'])[:13],16)],
            history=[rows[1]['transition']])
        request['history'][0]['predecessor']+=1
        with self.assertRaises(ValueError):host.clock(request)
        before=host.path.read_bytes()
        with self.assertRaises(ValueError):host.step(0,'different')
        self.assertEqual(before,host.path.read_bytes())
    def test_worker_binding_drift_and_checkpoint_tamper(self):
        clone=self.root/'clock';shutil.copy2(self.config['clock_worker'],clone)
        self.config['clock_worker']=str(clone);host=Framework(self.config,self.root/'store');host.initialize()
        clone.write_bytes(clone.read_bytes()+b'drift')
        with self.assertRaisesRegex(ValueError,'bytes changed'):host.inspect()
        clone.write_bytes(clone.read_bytes()[:-5]);host=Framework(self.config,self.root/'store')
        with closing(sqlite3.connect(host.path)) as db, db:
            cp=json.loads(db.execute('SELECT payload FROM checkpoints WHERE revision=0').fetchone()[0]);cp['revision']=1
            db.execute('UPDATE checkpoints SET payload=? WHERE revision=0',(canonical(cp).decode(),))
        with self.assertRaisesRegex(ValueError,'chain integrity'):host.inspect()
    def test_failure_and_operation_conflict_do_not_publish(self):
        host=Framework(self.config,self.root/'store');host.initialize();before=host.path.read_bytes()
        def fail(cp):raise RuntimeError('before publication')
        with self.assertRaises(RuntimeError):host.step(0,'first',before_publish=fail)
        self.assertEqual(before,host.path.read_bytes());self.assertEqual(host.inspect()['revision'],0)
        receipt=host.step(0,'first');self.assertEqual(host.step(0,'first'),receipt)
        with self.assertRaisesRegex(ValueError,'identity conflict'):host.step(1,'first')
        self.assertEqual(host.inspect()['revision'],1)
    def test_database_failure_rolls_back_both_checkpoints_and_receipt(self):
        host=Framework(self.config,self.root/'store');host.initialize()
        with closing(sqlite3.connect(host.path)) as db, db:
            db.execute("CREATE TRIGGER publication_failure BEFORE UPDATE ON metadata BEGIN SELECT RAISE(ABORT,'injected'); END")
        before=host.path.read_bytes()
        with self.assertRaises(sqlite3.IntegrityError):host.step(0,'failed-publication')
        self.assertEqual(before,host.path.read_bytes());self.assertEqual(host.inspect()['revision'],0)
        with closing(sqlite3.connect(host.path)) as db, db:
            self.assertEqual(db.execute('SELECT COUNT(*) FROM operations').fetchone()[0],0)
if __name__=='__main__':unittest.main()
