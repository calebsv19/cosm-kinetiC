"""Durable bounded retention mechanics, not synthetic CFD evidence."""
import hashlib,json,sqlite3,subprocess,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.checkpoint_store import CheckpointStore,sha

class StoreTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name).resolve()/'run'
    def tearDown(self):self.tmp.cleanup()
    def store(self,end=8,pins=(4,8),**kw):
        return CheckpointStore(self.root,'a'*64,end_step=end,selected_steps=pins,
                               consumer_check=lambda _:[],**kw)
    def append(self,s,step,**kw):
        rows=s.records();head=rows[-1] if rows else None
        return s.append(('state:%s'%step).encode(),step=step,checkpoint_digest=sha(str(step).encode()),
                        parent_checkpoint_digest=head['checkpoint_digest'] if head else None,
                        expected_revision=head['revision'] if head else None,observations={'time_s':step*.005},**kw)
    def test_640_metadata_steps_bounded_payloads_reopen(self):
        s=self.store(640,(80,160,240,320,480,640));s.create();peak=0
        for step in range(641):
            self.append(s,step);peak=max(peak,len(list(s.blobs.iterdir())))
        self.assertEqual(len(s.records()),641);self.assertLessEqual(peak,7)
        reopened=self.store(640,(80,160,240,320,480,640))
        self.assertEqual(reopened.read()[1],b'state:640')
        self.assertEqual(reopened.audit()['retained_payloads'],7)
        self.assertLess(reopened.audit()['bytes'],16*1024**2)
        self.assertEqual(reopened.read(80)[1],b'state:80')
        with self.assertRaisesRegex(ValueError,'pruned'):reopened.read(79)
    def test_crash_before_and_after_commit_reconcile_same_root(self):
        s=self.store();s.create();self.append(s,0)
        def before(stage):
            if stage=='before_commit':raise RuntimeError(stage)
        with self.assertRaises(RuntimeError):self.append(s,1,fault=before)
        self.assertEqual(s.read()[0]['step'],0);self.assertEqual(len(s.reconcile()),1)
        self.append(s,1)
        def after(stage):
            if stage=='after_commit':raise RuntimeError(stage)
        with self.assertRaises(RuntimeError):self.append(s,2,fault=after)
        self.assertEqual(s.read()[0]['step'],2);self.assertEqual(len(s.reconcile()),1)
        self.assertFalse(s.audit()['uncommitted_payloads'])
    def test_process_exit_commit_boundaries(self):
        s=self.store();s.create();self.append(s,0)
        script="""import os,sys
from coupling.checkpoint_store import CheckpointStore,sha
s=CheckpointStore(sys.argv[1],'a'*64,end_step=8,selected_steps=[4,8],consumer_check=lambda _:[])
r=s.records()[-1]
def die(stage):
 if stage==sys.argv[2]:os._exit(73)
s.append(b'crash-state',step=r['step']+1,checkpoint_digest=sha(b'crash'),parent_checkpoint_digest=r['checkpoint_digest'],expected_revision=r['revision'],observations={},fault=die)
"""
        import os
        env=dict(os.environ,PYTHONPATH=str(Path(__file__).resolve().parents[1]/'scripts'),PYTHONDONTWRITEBYTECODE='1')
        for stage,expected in [('before_commit',0),('after_commit',1)]:
            p=subprocess.run([sys.executable,'-c',script,str(self.root),stage],env=env,capture_output=True,text=True)
            self.assertEqual(p.returncode,73,p.stderr)
            reopened=self.store();self.assertEqual(reopened.read()[0]['step'],expected)
            reopened.reconcile();self.assertFalse(reopened.audit()['uncommitted_payloads'])

    def test_tamper_blob_and_record_refuse(self):
        s=self.store();s.create();self.append(s,0)
        p=next(s.blobs.iterdir());p.write_bytes(b'state:X')
        with self.assertRaisesRegex(ValueError,'SHA256'):s.read()
        p.write_bytes(b'state:0')
        with sqlite3.connect(s.path) as db:db.execute("update records set record=replace(record,'time_s','time_X')")
        with self.assertRaisesRegex(ValueError,'digest'):s.records()
    def test_lease_blocks_writer_and_wrong_expected_revision(self):
        s=self.store();s.create();self.append(s,0)
        with s.lease():
            with self.assertRaisesRegex(ValueError,'active reader'):self.append(s,1)
            self.assertEqual(s.read()[0]['step'],0)
        r=s.records()[0]
        with self.assertRaisesRegex(ValueError,'concurrent'):
            s.append(b'x',step=1,checkpoint_digest='b'*64,parent_checkpoint_digest=r['checkpoint_digest'],
                     expected_revision=9,observations={})
    def test_capacity_selection_policy_and_unsafe_path(self):
        with self.assertRaisesRegex(ValueError,'transient'):self.store(limit=1024)
        s=self.store(payload_limit=100,genesis_limit=50,index_limit=1024**2);s.create();self.append(s,0)
        with self.assertRaisesRegex(ValueError,'payload capacity'):
            s.append(b'x'*101,step=1,checkpoint_digest='b'*64,parent_checkpoint_digest=s.records()[0]['checkpoint_digest'],
                     expected_revision=0,observations={})
        with self.assertRaisesRegex(ValueError,'skipped'):self.append(s,5)
        outside=Path(self.tmp.name)/'link';outside.symlink_to(self.root,target_is_directory=True)
        with self.assertRaisesRegex(ValueError,'physical'):CheckpointStore(outside,'a'*64,end_step=8,selected_steps=[8])
        with self.assertRaises(FileExistsError):s.create()
    def test_active_external_consumer_preserves_uncommitted_payload(self):
        s=self.store();s.create();self.append(s,0);self.append(s,1)
        s.consumer_check=lambda _:['held']
        with self.assertRaisesRegex(ValueError,'active consumer'):self.append(s,2)
        self.assertEqual(s.read()[0]['step'],2) # commit succeeded; never blindly retry native work
        self.assertTrue(s.audit()['uncommitted_payloads'])
        s.consumer_check=lambda _:[];s.reconcile();self.assertFalse(s.audit()['uncommitted_payloads'])
if __name__=='__main__':unittest.main()
