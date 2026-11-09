"""Actual local runner/solver cancellation and same-identity lost-reply readback."""
import hashlib,json,os,subprocess,tempfile,time,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class DurableJobs(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.compiler_temp=tempfile.TemporaryDirectory();cls.interposer=Path(cls.compiler_temp.name)/'summary.dylib'
  subprocess.run(['/usr/bin/clang','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-dynamiclib',str(ROOT/'tests/l3_summary_barrier.c'),'-o',str(cls.interposer)],check=True)
 @classmethod
 def tearDownClass(cls):cls.compiler_temp.cleanup()
 def setUp(self):
  self.binary=Path(os.environ['PHYSICS_SIM_JOB_RUNNER_BIN']).resolve()
  self.temp=tempfile.TemporaryDirectory();self.base=Path(self.temp.name).resolve();self.jobs=self.base/'jobs';self.jobs.mkdir();self.job='l3-durable';self.slot=self.jobs/self.job
  self.addCleanup(self.cleanup)
 def cleanup(self):
  if self.slot.exists():
   for _ in range(100):
    subprocess.run([str(self.binary),'cancel','--job-id',self.job,'--jobs-root',str(self.jobs)],capture_output=True,timeout=10)
    summary=self.slot/'result_summary.json'
    if summary.exists() and summary.stat().st_size:break
    if getattr(self,'expected_publication_hold',False):
     progress=self.slot/'run_progress.json'
     if progress.exists():
      try:
       if json.loads(progress.read_text()).get('status')=='failed':break
      except json.JSONDecodeError:pass
    time.sleep(.02)
   else:
    self.temp._finalizer.detach()
    raise RuntimeError(f'Fixture still active or uncertain; retained {self.base}')
   pid_file=self.slot/'pid.txt'
   if pid_file.exists():
    pid=int(pid_file.read_text())
    for _ in range(100):
     try:os.kill(pid,0)
     except ProcessLookupError:break
     time.sleep(.01)
    else:
     self.temp._finalizer.detach()
     raise RuntimeError(f'Fixture PID still present; retained {self.base}')
  self.temp.cleanup()
 def command(self,mode,*extra,discard=False):
  command=[str(self.binary),mode,'--jobs-root',str(self.jobs)]
  command += ['--request',str(self.base/'job.json')] if mode=='submit' else ['--job-id',self.job]
  env=dict(os.environ)
  if mode=='submit' and getattr(self,'barrier_mode',None):env.update(DYLD_INSERT_LIBRARIES=str(self.interposer),PHYSICS_L3_BARRIER=self.barrier_mode,PHYSICS_L3_BARRIER_ROOT=str(self.base))
  return subprocess.run(command+list(extra),env=env,stdout=subprocess.DEVNULL if discard else subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=10)
 def submit(self,frames=2,discard=False,steps=2):
  scene=ROOT/'tests/fixtures/runtime_scene_primitive_retained.json'
  config={'schema_version':'physics_sim_headless_request_v1','runtime_scene_path':str(scene),'output_root':str(self.base/'ignored'),'frames':frames,'sim_steps_per_frame':steps,'progress_interval':1,'grid':'8x8x8','save_volume_frames':False,'save_render_frames':False,'skip_present':True,'overwrite':False}
  (self.base/'request.json').write_text(json.dumps(config))
  bundle={'schema_family':'codework_job','schema_variant':'headless_bundle_v1','job_id':self.job,'program':'physics_sim','tool':{'name':'physics_sim_headless','version':'0.1.0','target_os':'linux','target_arch':'x86_64'},'scene_payload':{'schema_family':'codework_scene','schema_variant':'scene_runtime_v1','path':str(scene)},'run_config':{'schema_family':'physics_sim_request','schema_variant':'physics_sim_headless_request_v1','path':'request.json'},'outputs':{'root':'.','report_path':'output/report.json','logs_dir':'.','artifacts_dir':'output/artifacts'},'metadata':{'title':'L3','description':'Local lifetime proof','created_by':'test','created_at':'2026-11-09T00:00:00Z'}}
  (self.base/'job.json').write_text(json.dumps(bundle));result=self.command('submit',discard=discard);self.assertEqual(result.returncode,0,result.stderr)
 def observed_command(self,mode,discard=False):
  for _ in range(30):
   result=self.command(mode,discard=discard)
   if result.returncode==0 or 'job refresh held; metadata not fully published' not in result.stderr:return result
   # Live generation contention is an explicit refusal. Keep this exact job ID.
   time.sleep(.01)
  return result
 def status(self):
  result=self.observed_command('status');self.assertEqual(result.returncode,0,result.stderr);return json.loads(result.stdout)
 def terminal(self):
  for _ in range(200):
   row=self.status()
   if row['state'] in ('completed','failed','cancelled'):return row
   time.sleep(.025)
  self.fail('job did not reach terminal state')
 def commit_boundary(self,mode):
  self.barrier_mode=mode;self.submit()
  try:
   for _ in range(200):
    if (self.base/'entered').exists():break
    time.sleep(.01)
   self.assertTrue((self.base/'entered').exists(),'actual summary commit barrier not reached')
   before=self.status();self.assertEqual(before['frames_completed'],2)
   result=self.observed_command('cancel');self.assertEqual(result.returncode,0,result.stderr)
   self.assertEqual(json.loads(result.stdout)['status'],'cancel_requested')
  finally:(self.base/'release').touch()
  after=self.terminal();self.assertEqual(after['state'],'completed');self.assertEqual(after['frames_completed'],2)
  self.assertEqual(json.loads(self.command('cancel').stdout)['status'],'already_terminal')
 def test_cancel_immediately_before_summary_commit(self):self.commit_boundary('before')
 def test_cancel_immediately_after_summary_commit(self):self.commit_boundary('after')
 def test_summary_publication_failure_retains_prefix_and_holds_success(self):
  self.expected_publication_hold=True;self.barrier_mode='fail';self.submit()
  progress=self.slot/'run_progress.json'
  row={}
  for _ in range(200):
   try:row=json.loads(progress.read_text())
   except (FileNotFoundError,json.JSONDecodeError):pass
   if row.get('status')=='failed':break
   time.sleep(.01)
  self.assertEqual(row['status'],'failed');self.assertEqual(row['frames_completed'],2)
  result=self.command('status');self.assertNotEqual(result.returncode,0);self.assertIn('job refresh held',result.stderr)
  self.assertEqual((self.slot/'result_summary.json').stat().st_size,0)
  self.assertNotEqual(self.command('submit').returncode,0)
  self.assertEqual(len(list(self.jobs.iterdir())),1)
 def test_cancel_before_first_step(self):
  flag=self.base/'cancel.flag';flag.write_text('cancel')
  summary=self.base/'summary.json';progress=self.base/'progress.json'
  result=subprocess.run([str(self.binary.parent/'physics_sim_headless'),'--runtime-scene',str(ROOT/'tests/fixtures/runtime_scene_primitive_retained.json'),'--output-root',str(self.base/'artifacts'),'--frames','2','--grid','8x8x8','--sim-steps-per-frame','2','--cancel-flag',str(flag),'--summary',str(summary),'--progress',str(progress),'--skip-present'],text=True,capture_output=True,timeout=10)
  self.assertEqual(result.returncode,2,result.stderr);self.assertEqual(json.loads(summary.read_text())['frames_completed'],0)
  self.assertEqual(json.loads(progress.read_text())['sim_steps_completed_in_frame'],0)
 def test_cancel_reply_reports_request_only(self):
  self.submit(100000);result=self.observed_command('cancel');self.assertEqual(result.returncode,0,result.stderr)
  self.assertEqual(json.loads(result.stdout)['status'],'cancel_requested');self.assertEqual(self.terminal()['state'],'cancelled')
 def test_cancel_after_commit_preserves_success(self):
  self.submit();row=self.terminal();self.assertEqual(row['state'],'completed')
  summary=(self.slot/'result_summary.json').read_bytes()
  result=self.command('cancel');self.assertEqual(result.returncode,0,result.stderr)
  self.assertEqual(json.loads(result.stdout)['status'],'already_terminal')
  self.assertEqual(self.status()['state'],'completed');self.assertFalse((self.slot/'cancel_requested.flag').exists())
  self.assertEqual((self.slot/'result_summary.json').read_bytes(),summary)
 def test_cancel_accepted_prefix_and_lost_cancel_reply(self):
  self.submit(100000)
  for _ in range(200):
   before=self.status()
   if before['frames_completed']>0:break
   time.sleep(.01)
  self.assertGreater(before['frames_completed'],0)
  result=self.observed_command('cancel',discard=True);self.assertEqual(result.returncode,0,result.stderr)
  after=self.terminal();self.assertEqual(after['state'],'cancelled');self.assertGreaterEqual(after['frames_completed'],before['frames_completed']);self.assertLess(after['frames_completed'],100000)
  self.assertEqual(json.loads(self.command('cancel').stdout)['status'],'already_terminal')
  self.assertEqual(self.status()['frames_completed'],after['frames_completed'])
 def test_cancel_inside_frame_preserves_partial_steps(self):
  self.submit(frames=2,steps=100000)
  for _ in range(200):
   before=self.status()
   if before['sim_steps_completed_in_frame']>0:break
   time.sleep(.01)
  self.assertEqual(before['frames_completed'],0);self.assertGreater(before['sim_steps_completed_in_frame'],0)
  cancel=self.observed_command('cancel');self.assertEqual(cancel.returncode,0,cancel.stderr)
  after=self.terminal();self.assertEqual(after['state'],'cancelled');self.assertEqual(after['frames_completed'],0)
  self.assertGreaterEqual(after['sim_steps_completed_in_frame'],before['sim_steps_completed_in_frame'])
 def test_lost_submit_reply_same_identity_recovery_without_duplicate(self):
  self.submit(discard=True);row=self.terminal();self.assertEqual(row['state'],'completed')
  summary=(self.slot/'result_summary.json').read_bytes();pid=(self.slot/'pid.txt').read_bytes()
  result=self.command('submit');self.assertNotEqual(result.returncode,0);self.assertIn('collision',result.stderr)
  self.assertEqual((self.slot/'pid.txt').read_bytes(),pid);self.assertEqual((self.slot/'result_summary.json').read_bytes(),summary);self.assertEqual(len(list(self.jobs.iterdir())),1)
 def test_report_failure_holds_then_recovers_same_job(self):
  self.submit();self.terminal();report=self.slot/'output/report.json';saved=report.read_bytes();report.unlink();report.mkdir()
  result=self.command('status');self.assertNotEqual(result.returncode,0)
  report.rmdir();report.write_bytes(saved);self.assertEqual(self.status()['state'],'completed')
if __name__=='__main__':unittest.main()
