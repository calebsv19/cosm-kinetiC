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
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.framework import Framework
from coupling.scenario import baseline
from surface_sources.growth_fire_v1 import canonical,digest

class FrameworkTests(unittest.TestCase):
    def test_receiver_geometry_refuses_before_any_worker_or_store(self):
        mutations = [
            ('origin_m', [0.125, 0, 0], 'X origin_m'),
            ('spacing_m', [0.25, 0.125, 0.125], 'X spacing_m'),
            ('dimensions', [16, 4, 4], 'X dimensions'),
            ('layer', 1, 'source must lie'),
            ('fluid_mask', [0] + [1] * 63, 'declared solid'),
            ('injection_fluid_mask', [0] + [1] * 63, 'base mask'),
        ]
        for field, value, message in mutations:
            with self.subTest(field=field):
                config = copy.deepcopy(self.config)
                config['receiver'][field] = value
                if field == 'fluid_mask':
                    config['receiver']['injection_fluid_mask'] = value[:]
                store = self.root / ('bad-' + field)
                with patch('subprocess.run', side_effect=AssertionError('worker invoked')):
                    with self.assertRaisesRegex(ValueError, message):
                        Framework(config, store)
                self.assertFalse(store.exists())

    def test_native_solid_overlap_refused_with_permissive_receiver_mask(self):
        from coupling.inert_cases import recipe
        from coupling.receiver_geometry import validate
        workers = tuple(self.config[k] for k in
                        ('growth_root', 'fire_worker', 'fluid_worker', 'clock_worker'))
        config = recipe(workers, live=True)
        # Legacy receiver masks may include unrelated solids. Only the cells
        # actually addressed by source overlap weights must be native fluid.
        validate(config)
        config['source']['origin_m'] = [0.375, 0.375, 0]
        with self.assertRaisesRegex(ValueError, 'overlaps native gas solid'):
            validate(config)

    def test_offset_source_deposition_is_exact_and_centred(self):
        import math
        from coupling.allocation import allocate_window
        from coupling.receiver_geometry import validate
        from surface_sources.growth_fire_v1 import areas
        self.config['source']['origin_m'] = [0.125, 0.125, 0]
        source = self.config['source']
        weights = areas(source)
        frame = dict(config=source, surface=dict(area_m2=weights),
                     cells=dict(energy_transferred_j=weights, smoke_transferred_kg=weights),
                     totals=dict(energy_transferred_j=math.fsum(weights),
                                 smoke_transferred_kg=math.fsum(weights)))
        policy = dict(schema='physics_sim_surface_source_policy/v1', consumer_id='test',
                      model_id='test', source_config=source, stream_start_tick=0,
                      producer=dict(worker_sha256='0'*64, adapter_sha256='0'*64),
                      receiver=self.config['receiver'])
        window = dict(steps=[dict(left_n=0, left_d=1, right_n=1, right_d=1)])
        before = allocate_window(frame, policy, window)
        validate(self.config)
        self.assertEqual(allocate_window(frame, policy, window), before)
        energy = before[0]['energy_j']
        total = math.fsum(energy)
        self.assertEqual(total, frame['totals']['energy_transferred_j'])
        self.assertEqual(before[0]['smoke_kg'], energy)
        self.assertEqual(math.fsum(((i % 8) + 0.5) / 8 * v
                                  for i, v in enumerate(energy)) / total, 0.5)
        Framework(self.config, self.root / 'valid-offset')

    def test_short_joint_reopen_matches_continuous(self):
        continuous = Framework(self.config, self.root / 'continuous')
        segmented = Framework(self.config, self.root / 'segmented')
        self.assertEqual(continuous.initialize(), segmented.initialize())
        for revision in range(3):
            expected = continuous.step(revision, 'step-' + str(revision))
            segmented = Framework(self.config, self.root / 'segmented')
            self.assertEqual(segmented.step(revision, 'step-' + str(revision)), expected)
            self.assertEqual(segmented.inspect(), continuous.inspect())

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
        clone=self.root/'clock';shutil.copy2(self.config['clock_worker'],clone);clone.chmod(0o755)  # This test owns its writable mutation fixture.
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
