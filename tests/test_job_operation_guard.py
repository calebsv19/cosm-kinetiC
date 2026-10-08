"""Job-level operation ownership across cooperative reads, effects and release."""
import fcntl
import json
import os
from pathlib import Path
import selectors
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class JobGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t=tempfile.TemporaryDirectory();base=Path(cls.t.name);cls.binary=base/'probe'
        source=base/'probe.c';source.write_text(r"""#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_guard.h"
#include "app/physics_sim_job_file.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
int main(int argc,char **argv){if(argc!=3)return 3;PhysicsSimJobGuard guard;if(!physics_sim_job_guard_begin(argv[1],&guard))return 4;
if(strcmp(argv[2],"nested")==0){if(physics_sim_job_guard_begin(argv[1],&guard)||!physics_sim_job_guard_current())return 13;}
if(strcmp(argv[2],"pause")==0){puts("READY");fflush(stdout);sleep(30);}
if(strcmp(argv[2],"exec")==0){execl("/bin/sh","sh","-c","printf 'EXEC\\n'; exec sleep 30",(char*)0);return 9;}
if(strcmp(argv[2],"root")==0){char old[1200];snprintf(old,sizeof(old),"%s.old",argv[1]);rename(argv[1],old);mkdir(argv[1],0700);}
if(strcmp(argv[2],"parent")==0){char old[1200];snprintf(old,sizeof(old),"%s.old",guard.parent);rename(guard.parent,old);mkdir(guard.parent,0700);mkdir(argv[1],0700);}
if(strcmp(argv[2],"lock")==0){char lock[1200],old[1200];snprintf(lock,sizeof(lock),"%s/.physics-sim-job-operation.lock",argv[1]);snprintf(old,sizeof(old),"%s/.old-lock",argv[1]);rename(lock,old);FILE *f=fopen(lock,"wb");fputs("foreign",f);fclose(f);}
int changed=strcmp(argv[2],"root")==0||strcmp(argv[2],"parent")==0||strcmp(argv[2],"lock")==0;
char output[1200];snprintf(output,sizeof(output),"%s/new.json",argv[1]);PhysicsSimJobFile file;FILE *f=physics_sim_job_file_begin(output,1,&file);
if(changed){int held=!physics_sim_job_guard_current()&&!f;physics_sim_job_guard_end(&guard);return held?0:10;}
if(!f)return 11;fputs("{\"v\":1}",f);int valid=physics_sim_job_file_finish(&file,f)&&physics_sim_job_guard_check(&guard);physics_sim_job_guard_end(&guard);return valid?0:12;}
""")
        flags=shlex.split(subprocess.check_output([os.environ.get('PKG_CONFIG','pkg-config'),'--cflags','--libs','json-c'],text=True))
        flags.extend(['-I'+str(ROOT/'third_party/codework_shared/core/core_scene_compile/include'),'-I'+str(ROOT/'third_party/codework_shared/core/core_base/include'),str(ROOT/'third_party/codework_shared/core/core_scene_compile/src/core_scene_compile_digest.c')])
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),*[str(ROOT/'src/app'/n) for n in ['physics_sim_job_guard.c','physics_sim_job_file.c','physics_sim_job_json.c','physics_sim_headless_output.c']],*flags,'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.t.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name).resolve();self.root=self.base/'jobs/job-1';self.root.mkdir(parents=True)
    def invoke(self,mode='normal',root=None):return subprocess.run([str(self.binary),str(root or self.root),mode],capture_output=True,text=True,timeout=5)
    def child(self,mode='pause'):
        p=subprocess.Popen([str(self.binary),str(self.root),mode],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        def cleanup():
            if p.poll() is None:p.kill();p.wait(timeout=5)
            p.stdout.close();p.stderr.close()
        self.addCleanup(cleanup);selector=selectors.DefaultSelector();selector.register(p.stdout,selectors.EVENT_READ)
        try:self.assertTrue(selector.select(5),'No ownership readiness')
        finally:selector.close()
        self.assertEqual(p.stdout.readline().strip(),'EXEC' if mode=='exec' else 'READY');return p
    def test_same_job_held_but_sibling_independent(self):
        p=self.child();self.assertEqual(self.invoke().returncode,4);sibling=self.root.parent/'job-2';sibling.mkdir()
        self.assertEqual(self.invoke(root=sibling).returncode,0);self.assertFalse((self.root/'new.json').exists())
        p.kill();p.wait(timeout=5);self.assertEqual(self.invoke().returncode,0)
    def test_exec_releases_operation_descriptors(self):
        p=self.child('exec');self.assertIsNone(p.poll());self.assertEqual(self.invoke().returncode,0)
    def test_replaced_root_holds_all_later_metadata_effects(self):
        self.assertEqual(self.invoke('root').returncode,0);self.assertEqual(list(self.root.iterdir()),[])
        self.assertTrue((self.root.with_name('job-1.old')/'.physics-sim-job-operation.lock').exists())
    def test_replaced_parent_holds_all_later_metadata_effects(self):
        self.assertEqual(self.invoke('parent').returncode,0);self.assertEqual(list(self.root.iterdir()),[])
    def test_replaced_lock_holds_and_preserves_foreign_bytes(self):
        self.assertEqual(self.invoke('lock').returncode,0);self.assertEqual((self.root/'.physics-sim-job-operation.lock').read_bytes(),b'foreign');self.assertFalse((self.root/'new.json').exists())
    def test_link_special_hardlink_and_nonempty_locks_are_held(self):
        lock=self.root/'.physics-sim-job-operation.lock';foreign=self.base/'foreign';foreign.write_bytes(b'keep')
        lock.symlink_to(foreign);self.assertEqual(self.invoke().returncode,4);lock.unlink()
        os.mkfifo(lock);self.assertEqual(self.invoke().returncode,4);lock.unlink()
        os.link(foreign,lock);self.assertEqual(self.invoke().returncode,4);lock.unlink()
        lock.write_bytes(b'unknown');self.assertEqual(self.invoke().returncode,4);self.assertEqual(foreign.read_bytes(),b'keep')
    def test_nested_admission_does_not_destroy_existing_guard(self):
        self.assertEqual(self.invoke('nested').returncode,0)
        self.assertEqual(json.loads((self.root/'new.json').read_text()),{'v':1})
    def test_refresh_failure_is_propagated_before_cancellation(self):
        runner=os.environ.get('PHYSICS_SIM_JOB_RUNNER_BIN');self.assertTrue(runner)
        row={'job_id':'job-1','state':'running','stage':'running','progress_path':str(self.root/'run_progress.json'),
             'summary_path':str(self.root/'result_summary.json'),'stdout_path':str(self.root/'stdout.log'),'stderr_path':str(self.root/'stderr.log')}
        status=self.root/'job_status.json';status.write_text(json.dumps(row));before=status.read_bytes()
        (self.root/'run_progress.json').write_text(json.dumps({'status':'running','stage':'simulating'}))
        lock=self.root/'.physics-sim-job-metadata.lock'
        with lock.open('w+') as owner:
            fcntl.flock(owner,fcntl.LOCK_EX|fcntl.LOCK_NB)
            for mode in ('status','cancel'):
                result=subprocess.run([runner,mode,'--jobs-root',str(self.root.parent),'--job-id','job-1'],capture_output=True,text=True,timeout=5)
                self.assertNotEqual(result.returncode,0);self.assertIn('job refresh held',result.stderr);self.assertEqual(status.read_bytes(),before)
        self.assertFalse((self.root/'cancel_requested.flag').exists())

    def test_actual_runner_status_and_cancel_hold_before_read_or_write(self):
        runner=os.environ.get('PHYSICS_SIM_JOB_RUNNER_BIN');self.assertTrue(runner,'Actual runner binary required')
        status=self.root/'job_status.json';status.write_bytes(b'not-json');p=self.child();before=status.read_bytes()
        for mode in ('status','cancel'):
            result=subprocess.run([runner,mode,'--jobs-root',str(self.root.parent),'--job-id','job-1'],capture_output=True,text=True,timeout=5)
            self.assertNotEqual(result.returncode,0);self.assertIn('job operation ownership held',result.stderr);self.assertEqual(status.read_bytes(),before)
        self.assertFalse((self.root/'cancel_requested.flag').exists());self.assertEqual(list(self.root.glob('.headless-sidecar-*.pending')),[])
if __name__=='__main__':unittest.main()
