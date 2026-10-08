"""Actual native interrupted pairs, exact recovery plans and rollback resumption."""
import fcntl,json,os,subprocess,sys,tempfile,unittest
from pathlib import Path
from unittest import mock
import test_job_pair_transaction as native
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import job_pair_recovery as recovery
class RecoveryJobs(unittest.TestCase):
 @classmethod
 def setUpClass(cls):native.JobPairs.setUpClass();cls.binary=native.JobPairs.binary
 @classmethod
 def tearDownClass(cls):native.JobPairs.tearDownClass()
 setUp=native.JobPairs.setUp
 run_pair=native.JobPairs.run_pair
 seed=native.JobPairs.seed
 capsule=native.JobPairs.capsule
 def interrupted(self,boundary=2):self.seed();self.assertEqual(self.run_pair(str(boundary)).returncode,-9)
 def restored(self,status=True,report=True):
  for label,present,value in [('status',status,1),('report',report,3)]:
   target=self.root/('job_status.json' if label=='status' else 'output/report.json')
   self.assertEqual(target.exists(),present)
   if present:self.assertEqual(json.loads(target.read_text())['value'],value)
  self.assertFalse((self.root/native.HOLD).exists());self.assertTrue((self.capsule()/'rolled-back.json').exists())
 def test_readonly_plan_is_stable_and_rollback_restores_both_native_boundaries(self):
  original=self.root
  for boundary in (1,2):
   with self.subTest(boundary=boundary):
    self.root=original/f'case-{boundary}';(self.root/'output').mkdir(parents=True);self.interrupted(boundary)
    before={str(p.relative_to(self.root)):p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
    plan=recovery.plan(self.root);self.assertEqual(plan['plan_sha256'],recovery.plan(self.root)['plan_sha256'])
    after={str(p.relative_to(self.root)):p.read_bytes() for p in self.root.rglob('*') if p.is_file()};self.assertEqual(before,after)
    result=recovery.rollback(self.root,plan['plan_sha256']);self.assertEqual(result['status'],'rollback_content_verified');self.restored()
    self.assertTrue((self.capsule()/'new-status.json').exists());self.assertTrue((self.capsule()/'old-status.json').exists())
    self.assertEqual(json.loads((self.capsule()/'rollback-new-status.json').read_text())['value'],2)
  self.root=original
 def test_all_original_presence_combinations_restore_exact_absences(self):
  original=self.root
  for status,report in ((False,False),(False,True),(True,False),(True,True)):
   with self.subTest(status=status,report=report):
    self.root=original/f'case-{int(status)}-{int(report)}';(self.root/'output').mkdir(parents=True);self.seed()
    if not status:(self.root/'job_status.json').unlink()
    if not report:(self.root/'output/report.json').unlink()
    self.assertEqual(self.run_pair('2').returncode,-9);plan=recovery.plan(self.root);recovery.rollback(self.root,plan['plan_sha256']);self.restored(status,report)
  self.root=original
 def test_wrong_digest_and_same_byte_rewrite_hold_without_recovery_mutation(self):
  self.interrupted();plan=recovery.plan(self.root)
  with self.assertRaises(ValueError):recovery.rollback(self.root,'0'*64)
  old=self.capsule()/'old-status.json';old.write_bytes(old.read_bytes())
  with self.assertRaises(ValueError):recovery.rollback(self.root,plan['plan_sha256'])
  self.assertTrue((self.root/native.HOLD).exists());self.assertFalse((self.capsule()/'rollback-intent.json').exists())
 def test_sigkill_after_all_six_rollback_rename_boundaries_can_resume(self):
  original=self.root
  code="""import sys,os,signal
sys.path.insert(0,sys.argv[1]);import job_pair_recovery as r
rename=r.os.rename;calls=0
boundary=int(sys.argv[4])
def die(a,b):
 global calls
 rename(a,b);calls+=1
 if calls==boundary:os.kill(os.getpid(),signal.SIGKILL)
r.os.rename=die;r.rollback(sys.argv[2],sys.argv[3])
"""
  for boundary in range(1,7):
   with self.subTest(boundary=boundary):
    self.root=original/f'case-{boundary}';(self.root/'output').mkdir(parents=True);self.interrupted();plan=recovery.plan(self.root)
    p=subprocess.run([sys.executable,'-B','-c',code,str(ROOT/'scripts'),str(self.root),plan['plan_sha256'],str(boundary)],capture_output=True,text=True,timeout=10)
    self.assertEqual(p.returncode,-9,p.stderr);self.assertTrue((self.root/native.HOLD).exists())
    resumed=recovery.plan(self.root);recovery.rollback(self.root,resumed['plan_sha256']);self.restored()
    self.assertEqual(json.loads((self.capsule()/'rollback-new-report.json').read_text())['value'],4)
  self.root=original
 def test_partial_record_stage_is_retained_and_replan_resumes(self):
  self.interrupted();plan=recovery.plan(self.root);write=recovery.os.write;calls=0
  def partial(fd,data):
   nonlocal calls
   calls+=1
   if calls==1:write(fd,data[:len(data)//2]);raise OSError('controlled partial record write')
   return write(fd,data)
  with mock.patch.object(recovery.os,'write',partial):
   with self.assertRaises(OSError):recovery.rollback(self.root,plan['plan_sha256'])
  stages=list(self.capsule().glob('.rollback-record-*.pending'));self.assertEqual(len(stages),1);before=stages[0].read_bytes()
  resumed=recovery.plan(self.root);recovery.rollback(self.root,resumed['plan_sha256']);self.restored();self.assertEqual(stages[0].read_bytes(),before)
 def test_foreign_generation_and_snapshot_links_or_unknown_entries_hold(self):
  self.interrupted();target=self.root/'job_status.json';target.write_bytes(b'{"value":99}')
  with self.assertRaises(ValueError):recovery.plan(self.root)
  target.write_bytes(b'{"value":2}');old=self.capsule()/'old-status.json';foreign=self.root.parent/'foreign';os.link(old,foreign)
  with self.assertRaises(ValueError):recovery.plan(self.root)
  foreign.unlink();(self.capsule()/'unknown').write_bytes(b'keep')
  with self.assertRaises(ValueError):recovery.plan(self.root)
  self.assertTrue((self.root/native.HOLD).exists())
 def test_namespace_and_journal_scope_are_fail_closed(self):
  self.interrupted();hold=self.root/native.HOLD;before=hold.read_bytes();hold.write_text(json.dumps({'schema':'physics_sim_job_pair_pending_v1','attempt':'../../outside'}))
  with self.assertRaises(ValueError):recovery.plan(self.root)
  hold.write_bytes(before);journal=self.capsule()/'plan.json';row=json.loads(journal.read_text());row['report_target']='../outside';journal.write_text(json.dumps(row))
  with self.assertRaises(ValueError):recovery.plan(self.root)
 def test_operation_contention_holds_read_and_mutation(self):
  self.interrupted();plan=recovery.plan(self.root)
  with (self.root/native.LOCK if hasattr(native,'LOCK') else self.root/'.physics-sim-job-operation.lock').open('rb') as lock:
   fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
   with self.assertRaises(ValueError):recovery.plan(self.root)
   with self.assertRaises(ValueError):recovery.rollback(self.root,plan['plan_sha256'])
  self.assertTrue((self.root/native.HOLD).exists())
 def test_other_slot_same_byte_drift_during_stage_holds(self):
  self.interrupted();plan=recovery.plan(self.root);write=recovery.write_stage;target=self.root/'output/report.json'
  def drift(p,data):write(p,data);target.write_bytes(target.read_bytes())
  with mock.patch.object(recovery,'write_stage',drift):
   with self.assertRaises(ValueError):recovery.rollback(self.root,plan['plan_sha256'])
  self.assertTrue((self.root/native.HOLD).exists());self.assertFalse((self.capsule()/'rolled-back.json').exists())
 def test_cli_defaults_to_readonly_and_requires_digest_for_rollback(self):
  self.interrupted();command=[sys.executable,'-B',str(ROOT/'scripts/job_pair_recovery.py'),'--job-root',str(self.root)]
  preview=subprocess.run(command,capture_output=True,text=True,timeout=10);self.assertEqual(preview.returncode,0,preview.stdout);plan=json.loads(preview.stdout)
  missing=subprocess.run(command+['--rollback'],capture_output=True,text=True,timeout=10);self.assertEqual(missing.returncode,2)
  applied=subprocess.run(command+['--rollback','--expected-plan-sha256',plan['plan_sha256']],capture_output=True,text=True,timeout=10);self.assertEqual(applied.returncode,0,applied.stdout);self.restored()
 def test_source_checkout_storage_is_rejected_before_record_admission(self):
  repo=self.root.parent.parent/'repository';(repo/'.git').mkdir(parents=True);selected=repo/'src/job-1';selected.mkdir(parents=True)
  (selected/'.physics-sim-job-operation.lock').write_bytes(b'');(selected/native.HOLD).write_bytes(b'unknown')
  with self.assertRaisesRegex(ValueError,'protected source checkout storage'):recovery.plan(selected)
  self.assertEqual((selected/native.HOLD).read_bytes(),b'unknown')
 def test_source_boundary_change_during_recovery_holds_after_retained_stage(self):
  self.interrupted();plan=recovery.plan(self.root);write=recovery.write_stage;repo=self.root.parent.parent
  def changed(p,data):write(p,data);(repo/'.git').mkdir()
  with mock.patch.object(recovery,'write_stage',changed):
   with self.assertRaisesRegex(ValueError,'protected source checkout storage'):recovery.rollback(self.root,plan['plan_sha256'])
  self.assertTrue((self.root/native.HOLD).exists());self.assertFalse((self.capsule()/'rolled-back.json').exists())
  self.assertEqual(json.loads((self.root/'job_status.json').read_text())['value'],2)
 def test_v2_producer_inventory_rejects_changes_predating_first_readonly_plan(self):
  original=self.root
  for name in ('old-status.json','old-report.json','new-status.json','new-report.json','plan.json'):
   with self.subTest(name=name):
    self.root=original/name.replace('.','-');(self.root/'output').mkdir(parents=True);self.interrupted();source=self.capsule()/name
    if name=='plan.json':row=json.loads(source.read_text());row['root_inode']=self.root.stat().st_ino;source.write_text(json.dumps(row))
    else:source.write_bytes(b'{"value":99}')
    with self.assertRaisesRegex(ValueError,'digest|generation'):recovery.plan(self.root)
    self.assertTrue((self.root/native.HOLD).exists());self.assertFalse((self.capsule()/'rollback-intent.json').exists())
  self.root=original
 def test_legacy_v1_is_explicitly_identified_without_producer_inventory_claim(self):
  self.interrupted();capsule=self.capsule();source=capsule/'plan.json';record=json.loads(source.read_text());record['schema']='physics_sim_job_pair_v1';record.pop('retained_sha256');source.write_text(json.dumps(record))
  hold=self.root/native.HOLD;pending=json.loads(hold.read_text());pending['schema']='physics_sim_job_pair_pending_v1';pending.pop('plan_sha256');hold.write_text(json.dumps(pending))
  plan=recovery.plan(self.root);self.assertFalse(plan['producer_inventory_verified']);recovery.rollback(self.root,plan['plan_sha256']);self.restored()
if __name__=='__main__':unittest.main()
