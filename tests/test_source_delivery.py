"""Cutoff persistence/partition, unchanged omission, and atomic recovery admission."""
import copy,json,os,sqlite3,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path[:0]=[str(ROOT/'scripts'),str(ROOT.parent.parent/'bin')]
from coupling.framework import Framework
from coupling.inert_cases import recipe
from coupling.recovery_cases import configuration
from coupling.journal import Journal
from numeric_digest_stream import sealed
class SourceDeliveryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
    def setUp(self):
        self.root=Path(os.environ['RECOVERY_TEST_ROOT'])/self._testMethodName;self.root.mkdir(parents=True,exist_ok=False)
        self.old=os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT');os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(self.root/'attempts')
        self.addCleanup(lambda:os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT',None) if self.old is None else os.environ.__setitem__('PHYSICS_SIM_EXPERIMENT_ROOT',self.old))
    def config(self):
        c=recipe(self.workers,live=True);c['windows']=3;c['source_delivery']={'schema':'fire_fluid_delivery_cutoff/v1','active_windows':1};return c
    def test_cutoff_restart_fault_partition_and_tamper(self):
        c=self.config();f=Framework(c,self.root/'run');f.initialize();f.step(0,'w0');parent=f.inspect();q=parent['fluid']['state']['data']['input_energy_j'];self.assertGreater(q,0)
        staged=[]
        def fault(cp):staged.append(cp['digest']);raise RuntimeError('cutoff prepublication fault')
        with self.assertRaisesRegex(RuntimeError,'cutoff prepublication'):f.step(1,'w1',before_publish=fault)
        self.assertEqual(f.inspect()['digest'],parent['digest']);receipt=f.step(1,'w1');self.assertEqual(receipt['checkpoint_digest'],staged[0]);self.assertEqual(f.step(1,'w1'),receipt)
        f=Framework(c,self.root/'run');f.step(2,'w2');cp=f.inspect();self.assertEqual(cp['fluid']['state']['data']['input_energy_j'],q)
        d=cp['evidence']['source_delivery'];self.assertFalse(d['active']);self.assertEqual(d['delivered']['energy_transferred_j'],0);self.assertGreater(d['diverted']['energy_transferred_j'],0)
        self.assertEqual(d['available'],d['diverted']);self.assertEqual(cp['fluid']['state']['data']['inert_object']['steps'],60)
        # Fresh owned corrupt fixture; the good journal is untouched.
        bad=self.root/'tamper';bad.mkdir()
        with sqlite3.connect(f.path) as src,sqlite3.connect(bad/'joint.sqlite3') as db:
            src.backup(db);j=Journal(c['journal_codec']);p=j.decode(db.execute('select payload from checkpoints where revision=3').fetchone()[0]);p['evidence']['source_delivery']['diverted']['energy_transferred_j']+=1;p['evidence']['source_delivery']=sealed(p['evidence']['source_delivery']);p=sealed(p);db.execute('update checkpoints set payload=? where revision=3',(j.encode(p),));db.commit()
        with self.assertRaisesRegex(ValueError,'persisted source delivery'):Framework(c,bad).inspect()
        c2=copy.deepcopy(c);c2['source_delivery']['active_windows']=2
        with self.assertRaisesRegex(ValueError,'binding mismatch'):Framework(c2,self.root/'run').inspect()
    def test_omitted_policy_reproduces_retained_native_states(self):
        c=recipe(self.workers,live=True);f=Framework(c,self.root/'run');f.initialize()
        path=Path(os.environ['RECOVERY_LEGACY_JOURNAL']);j=Journal(c['journal_codec'])
        with sqlite3.connect(path.resolve().as_uri()+'?mode=ro',uri=True) as db:
            old={n:j.decode(p) for n,p in db.execute('select revision,payload from checkpoints where revision<=3')}
        for i in range(3):
            f.step(i,'window-'+str(i));cp=f.inspect();self.assertEqual(cp['source'],old[i+1]['source']);self.assertEqual(cp['fluid']['state'],old[i+1]['fluid']['state']);self.assertNotIn('source_delivery',cp['evidence'])
    def test_bad_policy_refuses_before_database(self):
        for value in (0,3,True,1.5):
            c=self.config();c['source_delivery']['active_windows']=value
            with self.assertRaises(ValueError):Framework(c,self.root/'bad')
        self.assertFalse((self.root/'bad').exists())
    def test_matrix_keeps_physical_source_and_correct_masks(self):
        cases=[configuration(self.workers,n) for n in ('continuous-16','continuous-32','continuous-32-half','wind-off-32')]
        from coupling.solid_box import topology
        for c in cases:
            self.assertEqual(c['source'],cases[0]['source']);self.assertEqual(c['fluid']['inert_object'],cases[0]['fluid']['inert_object']);self.assertEqual(c['periods'][2],[2,5]);self.assertEqual(c['windows'],32)
            mask=topology(c['fluid']);self.assertEqual(c['receiver']['injection_fluid_mask'],[1-v for v in mask[:len(c['receiver']['injection_fluid_mask'])]])
if __name__=='__main__':unittest.main()
