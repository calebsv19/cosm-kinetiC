"""Verified v2 forward recovery, retained prior bytes and direction-bound resume."""
import json,os,subprocess,sys,unittest
from pathlib import Path
from unittest import mock
import test_job_pair_transaction as native
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import job_pair_recovery as recovery
class ForwardJobs(unittest.TestCase):
 @classmethod
 def setUpClass(cls):native.JobPairs.setUpClass();cls.binary=native.JobPairs.binary
 @classmethod
 def tearDownClass(cls):native.JobPairs.tearDownClass()
 setUp=native.JobPairs.setUp
 run_pair=native.JobPairs.run_pair
 seed=native.JobPairs.seed
 capsule=native.JobPairs.capsule
 def held(self,mode='die-before'):self.seed();self.assertEqual(self.run_pair(mode).returncode,-9)
 def complete(self):
  self.assertEqual(json.loads((self.root/'job_status.json').read_text())['value'],2);self.assertEqual(json.loads((self.root/'output/report.json').read_text())['value'],4)
  self.assertFalse((self.root/native.HOLD).exists());self.assertTrue((self.capsule()/'forwarded.json').exists());self.assertFalse((self.capsule()/'rolled-back.json').exists())
 def test_native_pre_and_post_promotion_states_install_exact_new_bytes(self):
  original=self.root
  for mode in ('die-before','1','2'):
   with self.subTest(mode=mode):
    self.root=original/f'case-{mode}';(self.root/'output').mkdir(parents=True);self.held(mode);plan=recovery.plan(self.root)
    self.assertTrue(plan['forward_promotion_allowed']);self.assertTrue(plan['producer_inventory_verified'])
    result=recovery.forward(self.root,plan['plan_sha256']);self.assertEqual(result['status'],'forward_content_verified');self.complete()
    self.assertEqual((self.capsule()/'old-status.json').read_bytes(),b'{"value":1}');self.assertEqual((self.capsule()/'old-report.json').read_bytes(),b'{"value":3}')
  self.root=original
 def test_all_old_presence_combinations_install_and_retain_recorded_prior_states(self):
  original=self.root
  for status,report in ((False,False),(False,True),(True,False),(True,True)):
   with self.subTest(status=status,report=report):
    self.root=original/f'case-{int(status)}-{int(report)}';(self.root/'output').mkdir(parents=True);self.seed()
    if not status:(self.root/'job_status.json').unlink()
    if not report:(self.root/'output/report.json').unlink()
    self.assertEqual(self.run_pair('die-before').returncode,-9);plan=recovery.plan(self.root);recovery.forward(self.root,plan['plan_sha256']);self.complete()
    for label,present in (('status',status),('report',report)):
     self.assertEqual((self.capsule()/f'old-{label}.json').exists(),present)
     self.assertEqual((self.capsule()/f'forward-old-{label}.json').exists(),present)
  self.root=original
 def test_sigkill_after_all_six_forward_rename_boundaries_can_resume(self):
  original=self.root
  code="""import sys,os,signal
sys.path.insert(0,sys.argv[1]);import job_pair_recovery as r
rename=r.os.rename;calls=0;boundary=int(sys.argv[4])
def die(a,b):
 global calls
 rename(a,b);calls+=1
 if calls==boundary:os.kill(os.getpid(),signal.SIGKILL)
r.os.rename=die;r.forward(sys.argv[2],sys.argv[3])
"""
  for boundary in range(1,7):
   with self.subTest(boundary=boundary):
    self.root=original/f'case-{boundary}';(self.root/'output').mkdir(parents=True);self.held();plan=recovery.plan(self.root)
    p=subprocess.run([sys.executable,'-B','-c',code,str(ROOT/'scripts'),str(self.root),plan['plan_sha256'],str(boundary)],capture_output=True,text=True,timeout=10)
    self.assertEqual(p.returncode,-9,p.stderr);self.assertTrue((self.root/native.HOLD).exists());self.assertEqual(self.run_pair().returncode,4)
    resumed=recovery.plan(self.root);self.assertEqual(resumed['selected_direction'],'forward');self.assertFalse(resumed['rollback_allowed'])
    recovery.forward(self.root,resumed['plan_sha256']);self.complete()
    self.assertEqual((self.capsule()/'forward-old-status.json').read_bytes(),b'{"value":1}');self.assertEqual((self.capsule()/'forward-old-report.json').read_bytes(),b'{"value":3}')
  self.root=original
 def test_committed_directions_cannot_be_switched(self):
  original=self.root;rename=recovery.os.rename
  for direction in ('rollback','forward'):
   with self.subTest(direction=direction):
    self.root=original/f'case-{direction}';(self.root/'output').mkdir(parents=True);self.held();plan=recovery.plan(self.root)
    def stop(a,b):rename(a,b);raise OSError('controlled post-intent interruption')
    with mock.patch.object(recovery.os,'rename',stop):
     with self.assertRaises(OSError):getattr(recovery,direction)(self.root,plan['plan_sha256'])
    resumed=recovery.plan(self.root);self.assertEqual(resumed['selected_direction'],direction)
    other='forward' if direction=='rollback' else 'rollback'
    with self.assertRaises(ValueError):getattr(recovery,other)(self.root,resumed['plan_sha256'])
    self.assertFalse((self.capsule()/f'{other}-intent.json').exists());self.assertTrue((self.root/native.HOLD).exists())
  self.root=original
 def test_legacy_v1_cannot_forward_even_if_current_is_already_new(self):
  self.held('2');c=self.capsule();journal=c/'plan.json';row=json.loads(journal.read_text());row['schema']='physics_sim_job_pair_v1';row.pop('retained_sha256');journal.write_text(json.dumps(row))
  hold=self.root/native.HOLD;row=json.loads(hold.read_text());row['schema']='physics_sim_job_pair_pending_v1';row.pop('plan_sha256');hold.write_text(json.dumps(row))
  plan=recovery.plan(self.root);self.assertFalse(plan['forward_promotion_allowed'])
  with self.assertRaises(ValueError):recovery.forward(self.root,plan['plan_sha256'])
  self.assertTrue(hold.exists());self.assertFalse((c/'forward-intent.json').exists())
 def test_wrong_plan_digest_preserves_current_and_hold(self):
  self.held();before=(self.root/'job_status.json').read_bytes()
  with self.assertRaises(ValueError):recovery.forward(self.root,'0'*64)
  self.assertEqual((self.root/'job_status.json').read_bytes(),before);self.assertTrue((self.root/native.HOLD).exists());self.assertFalse((self.capsule()/'forward-intent.json').exists())
 def test_partial_forward_intent_stage_is_retained_and_resume_uses_fresh_stage(self):
  self.held();plan=recovery.plan(self.root);write=recovery.os.write;calls=0
  def partial(fd,data):
   nonlocal calls
   calls+=1
   if calls==1:write(fd,data[:len(data)//2]);raise OSError('controlled partial forward receipt')
   return write(fd,data)
  with mock.patch.object(recovery.os,'write',partial):
   with self.assertRaises(OSError):recovery.forward(self.root,plan['plan_sha256'])
  stages=list(self.capsule().glob('.forward-record-*.pending'));self.assertEqual(len(stages),1);before=stages[0].read_bytes()
  resumed=recovery.plan(self.root);recovery.forward(self.root,resumed['plan_sha256']);self.complete();self.assertEqual(stages[0].read_bytes(),before)
 def test_other_slot_drift_during_forward_stage_holds(self):
  self.held();plan=recovery.plan(self.root);write=recovery.write_stage;target=self.root/'output/report.json'
  def changed(p,data):write(p,data);target.write_bytes(target.read_bytes())
  with mock.patch.object(recovery,'write_stage',changed):
   with self.assertRaises(ValueError):recovery.forward(self.root,plan['plan_sha256'])
  self.assertTrue((self.root/native.HOLD).exists());self.assertFalse((self.capsule()/'forwarded.json').exists())
 def test_unexplained_current_absence_is_not_filled_by_forward(self):
  self.held();(self.root/'job_status.json').unlink()
  with self.assertRaisesRegex(ValueError,'unexplained absent'):recovery.plan(self.root)
  self.assertTrue((self.root/native.HOLD).exists());self.assertFalse((self.capsule()/'forward-intent.json').exists())
 def test_cli_forward_requires_exact_digest_and_modes_are_mutually_exclusive(self):
  self.held();command=[sys.executable,'-B',str(ROOT/'scripts/job_pair_recovery.py'),'--job-root',str(self.root)]
  preview=subprocess.run(command,capture_output=True,text=True,timeout=10);self.assertEqual(preview.returncode,0,preview.stdout);plan=json.loads(preview.stdout)
  for flags in (['--forward'],['--forward','--rollback']):self.assertEqual(subprocess.run(command+flags,capture_output=True,text=True,timeout=10).returncode,2)
  applied=subprocess.run(command+['--forward','--expected-plan-sha256',plan['plan_sha256']],capture_output=True,text=True,timeout=10);self.assertEqual(applied.returncode,0,applied.stdout);self.complete()
if __name__=='__main__':unittest.main()
