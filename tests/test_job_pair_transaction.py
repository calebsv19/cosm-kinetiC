"""Native fixed-pair retention, interrupted promotion and fail-closed admission."""
import hashlib,json,os,shlex,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
HOLD='.physics-sim-job-publication.pending'
class JobPairs(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.t=tempfile.TemporaryDirectory();base=Path(cls.t.name);source=base/'probe.c';obj=base/'output.o';cls.binary=base/'probe'
  source.write_text(r'''#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_file.h"
#include "app/physics_sim_job_guard.h"
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
static int boundary, promotions, fail_second, change_hold, die_before;
int pair_test_renameat(int a,const char *old,int b,const char *name){
 if(strcmp(name,"job_status.json")==0||strcmp(name,"report.json")==0){
  if(die_before&&promotions==0)kill(getpid(),SIGKILL);
  if(fail_second&&promotions==1){errno=EIO;return -1;}
  int result=renameat(a,old,b,name);if(result==0){++promotions;
   if(change_hold&&promotions==1){int fd=openat(a,PHYSICS_SIM_JOB_PAIR_PENDING,O_WRONLY|O_TRUNC);if(fd<0)_exit(20);if(write(fd,"foreign hold",12)!=12)_exit(21);close(fd);}
   if(promotions==boundary)kill(getpid(),SIGKILL);
  }return result;
 }
 return renameat(a,old,b,name);
}
int main(int argc,char **argv){if(argc!=3)return 3;boundary=atoi(argv[2]);fail_second=strcmp(argv[2],"fail-second")==0;change_hold=strcmp(argv[2],"change-hold")==0;die_before=strcmp(argv[2],"die-before")==0;
 PhysicsSimJobGuard guard;if(!physics_sim_job_guard_begin(argv[1],&guard))return 4;
 char first[1200],second[1200];snprintf(first,sizeof(first),"%s/job_status.json",argv[1]);snprintf(second,sizeof(second),"%s/output/report.json",argv[1]);
 PhysicsSimJobFile a,b;FILE *x=physics_sim_job_file_begin(first,1,&a);if(!x)return 5;fputs("{\"value\":2}",x);if(!physics_sim_job_file_prepare(&a,x))return 6;
 FILE *y=physics_sim_job_file_begin(second,1,&b);if(!y){physics_sim_job_file_discard(&a,x);return 7;}
 fputs("{\"value\":4}",y);if(!physics_sim_job_file_prepare(&b,y)){physics_sim_job_file_discard(&a,x);return 8;}
 int ok=physics_sim_job_file_publish_pair(&a,x,&b,y);physics_sim_job_guard_end(&guard);return ok?0:9;
}''')
  flags=shlex.split(subprocess.check_output([os.environ.get('PKG_CONFIG','pkg-config'),'--cflags','--libs','json-c'],text=True))
  flags.extend(['-I'+str(ROOT/'third_party/codework_shared/core/core_scene_compile/include'),'-I'+str(ROOT/'third_party/codework_shared/core/core_base/include'),str(ROOT/'third_party/codework_shared/core/core_scene_compile/src/core_scene_compile_digest.c')])
  subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-Drenameat=pair_test_renameat','-c',str(ROOT/'src/app/physics_sim_headless_output.c'),'-o',str(obj)],check=True,capture_output=True)
  subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),str(obj),*[str(ROOT/'src/app'/n) for n in ['physics_sim_job_file.c','physics_sim_job_guard.c','physics_sim_job_json.c']],*flags,'-o',str(cls.binary)],check=True,capture_output=True)
 @classmethod
 def tearDownClass(cls):cls.t.cleanup()
 def setUp(self):
  self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.root=Path(self.t.name).resolve()/'jobs/job-1';(self.root/'output').mkdir(parents=True)
 def run_pair(self,mode='0'):return subprocess.run([str(self.binary),str(self.root),mode],capture_output=True,text=True,timeout=5)
 def seed(self):
  (self.root/'job_status.json').write_bytes(b'{"value":1}');(self.root/'output/report.json').write_bytes(b'{"value":3}')
 def capsule(self):
  rows=list(self.root.glob('.job-pair-*'));self.assertEqual(len(rows),1);return rows[0]
 def assert_saved(self,capsule,present=True):
  for name,value in [('new-status.json',2),('new-report.json',4)]:self.assertEqual(json.loads((capsule/name).read_text())['value'],value)
  for name,value in [('old-status.json',1),('old-report.json',3)]:
   self.assertEqual((capsule/name).exists(),present)
   if present:self.assertEqual(json.loads((capsule/name).read_text())['value'],value)
  plan=json.loads((capsule/'plan.json').read_text());self.assertEqual(plan['root_inode'],self.root.stat().st_ino);self.assertEqual(plan['old_status_present'],present);self.assertEqual(plan['old_report_present'],present)
  self.assertEqual(plan['schema'],'physics_sim_job_pair_v2')
  for name in ('old-status.json','old-report.json','new-status.json','new-report.json'):
   source=capsule/name;self.assertEqual(plan['retained_sha256'][name],hashlib.sha256(source.read_bytes()).hexdigest() if source.exists() else None)
  hold=self.root/HOLD
  if hold.exists() and hold.read_bytes()!=b'foreign hold':self.assertEqual(json.loads(hold.read_text())['plan_sha256'],hashlib.sha256((capsule/'plan.json').read_bytes()).hexdigest())
 def test_success_retains_all_generations_and_clears_only_hold(self):
  self.seed();self.assertEqual(self.run_pair().returncode,0);c=self.capsule();self.assert_saved(c)
  self.assertTrue((c/'completed.json').exists());self.assertFalse((self.root/HOLD).exists())
  self.assertEqual(json.loads((self.root/'job_status.json').read_text())['value'],2);self.assertEqual(json.loads((self.root/'output/report.json').read_text())['value'],4)
 def test_initial_publication_records_absent_predecessors(self):
  self.assertEqual(self.run_pair().returncode,0);self.assert_saved(self.capsule(),False);self.assertFalse((self.root/HOLD).exists())
 def test_forced_death_at_both_promotion_boundaries_retains_and_holds(self):
  for boundary in (1,2):
   with self.subTest(boundary=boundary):
    original=self.root;self.root=original/f'case-{boundary}';(self.root/'output').mkdir(parents=True);self.seed()
    self.assertEqual(self.run_pair(str(boundary)).returncode,-9);c=self.capsule();self.assert_saved(c)
    self.assertEqual(json.loads((self.root/HOLD).read_text())['attempt'],c.name);self.assertFalse((c/'completed.json').exists())
    self.assertEqual(self.run_pair().returncode,4);self.assertEqual(len(list(self.root.glob('.job-pair-*'))),1)
    self.assertEqual(json.loads((self.root/'job_status.json').read_text())['value'],2)
    self.assertEqual(json.loads((self.root/'output/report.json').read_text())['value'],3 if boundary==1 else 4)
    self.root=original
 def test_second_promotion_io_failure_retains_hold_and_old_report(self):
  self.seed();self.assertEqual(self.run_pair('fail-second').returncode,9);self.assert_saved(self.capsule())
  self.assertTrue((self.root/HOLD).exists());self.assertEqual(self.run_pair().returncode,4)
  self.assertEqual((self.root/'output/report.json').read_bytes(),b'{"value":3}')
 def test_changed_hold_is_preserved_and_blocks_second_promotion(self):
  self.seed();self.assertEqual(self.run_pair('change-hold').returncode,9);self.assert_saved(self.capsule())
  self.assertEqual((self.root/HOLD).read_bytes(),b'foreign hold');self.assertEqual(self.run_pair().returncode,4)
  self.assertEqual((self.root/'output/report.json').read_bytes(),b'{"value":3}')
 def test_unknown_hold_forms_block_before_new_stages(self):
  self.seed();hold=self.root/HOLD;outside=self.root.parent/'outside';outside.write_bytes(b'keep')
  for kind in ('empty','directory','symlink','hardlink'):
   with self.subTest(kind=kind):
    if kind=='empty':hold.write_bytes(b'')
    elif kind=='directory':hold.mkdir()
    elif kind=='symlink':hold.symlink_to(outside)
    else:os.link(outside,hold)
    self.assertEqual(self.run_pair().returncode,4);self.assertEqual(list(self.root.glob('.job-pair-*')),[])
    self.assertEqual((self.root/'job_status.json').read_bytes(),b'{"value":1}');self.assertEqual(outside.read_bytes(),b'keep')
    if kind=='directory':hold.rmdir()
    else:hold.unlink()
 def test_shared_sha_handles_maximum_metadata_size_and_reader_holds_excess(self):
  overhead=len(json.dumps({'padding':''}).encode());large=json.dumps({'padding':'x'*(1024*1024-overhead)}).encode()
  self.assertEqual(len(large),1024*1024)
  (self.root/'job_status.json').write_bytes(large);(self.root/'output/report.json').write_bytes(large)
  self.assertEqual(self.run_pair().returncode,0);c=self.capsule();plan=json.loads((c/'plan.json').read_text())
  for name in ('old-status.json','old-report.json'):self.assertEqual(plan['retained_sha256'][name],hashlib.sha256(large).hexdigest())
  original=self.root;self.root=original/'excess';(self.root/'output').mkdir(parents=True)
  too_large=large+b' ';(self.root/'job_status.json').write_bytes(too_large);(self.root/'output/report.json').write_bytes(b'{}')
  self.assertEqual(self.run_pair().returncode,9);self.assertEqual((self.root/'job_status.json').read_bytes(),too_large)
  self.assertEqual(list(self.root.glob('.job-pair-*')),[]);self.assertFalse((self.root/HOLD).exists());self.root=original
if __name__=='__main__':unittest.main()
