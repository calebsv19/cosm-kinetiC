"""Bounded startup channel observation and owned direct-child failure cleanup."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Startup(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t=tempfile.TemporaryDirectory();base=Path(cls.t.name);cls.binary=base/'probe';source=base/'probe.c'
        source.write_text(r"""#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_startup.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
int main(int argc,char **argv){if(argc!=2)return 3;PhysicsSimJobStartup startup;if(!physics_sim_job_startup_open(&startup))return 4;
if(strcmp(argv[1],"nonchild")==0){char diag[256];int ok=physics_sim_job_startup_wait(&startup,getpid(),100,diag,sizeof(diag));puts(diag);return ok?5:0;}
pid_t pid=fork();if(pid<0)return 6;if(!pid){physics_sim_job_startup_child(&startup);
if(strcmp(argv[1],"timeout")==0){sleep(20);_exit(0);}
if(strcmp(argv[1],"setup")==0){physics_sim_job_startup_error(&startup,2,EIO);_exit(126);}
if(strcmp(argv[1],"partial")==0){write(startup.writer,"xx",2);_exit(127);}
if(strcmp(argv[1],"exit")==0)_exit(126);
if(strcmp(argv[1],"exec-fail")==0){execl("/missing/physics-startup-worker","worker",(char*)0);physics_sim_job_startup_error(&startup,3,errno);_exit(127);}
if(strcmp(argv[1],"fast")==0)execl("/usr/bin/true","true",(char*)0);
else execl("/bin/sleep","sleep","0.1",(char*)0);physics_sim_job_startup_error(&startup,3,errno);_exit(127);}
if(strcmp(argv[1],"exit")==0){struct timespec pause={0,50000000};nanosleep(&pause,0);}
char diag[256];int valid=physics_sim_job_startup_wait(&startup,pid,100,diag,sizeof(diag));puts(diag);
int expected=strcmp(argv[1],"exec")==0||strcmp(argv[1],"fast")==0;
if(expected){int status;pid_t waited=waitpid(pid,&status,0);if(waited<0&&errno!=ECHILD)return 7;}
else{int status;if(waitpid(pid,&status,WNOHANG)!=-1||errno!=ECHILD)return 8;}
return valid==expected?0:9;}
""")
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),str(ROOT/'src/app/physics_sim_job_startup.c'),'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.t.cleanup()
    def invoke(self,mode):return subprocess.run([str(self.binary),mode],capture_output=True,text=True,timeout=3)
    def test_exec_closes_channel_without_setup_error(self):
        p=self.invoke('exec');self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('exec channel closed',p.stdout)
    def test_fast_successful_exit_is_compatible(self):
        p=self.invoke('fast');self.assertEqual(p.returncode,0,p.stdout+p.stderr)
    def test_exec_failure_reports_phase_and_reaps_child(self):
        p=self.invoke('exec-fail');self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('phase=3',p.stdout);self.assertIn('direct_child_reaped=true',p.stdout)
    def test_setup_failure_reports_phase_and_reaps_child(self):
        p=self.invoke('setup');self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('phase=2',p.stdout);self.assertIn('direct_child_reaped=true',p.stdout)
    def test_timeout_is_bounded_and_reaps_only_owned_child(self):
        p=self.invoke('timeout');self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('timed out',p.stdout);self.assertIn('direct_child_reaped=true',p.stdout)
    def test_partial_or_observed_nonzero_exit_is_held_and_reaped(self):
        for mode in ('partial','exit'):
            p=self.invoke(mode);self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('direct_child_reaped=true',p.stdout)
    def test_nonchild_pid_is_refused_without_signal(self):
        p=self.invoke('nonchild');self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('child ownership',p.stdout)
    def test_actual_submit_retains_exec_failure_in_job_status(self):
        selected=os.environ.get('PHYSICS_SIM_JOB_RUNNER_BIN');self.assertTrue(selected,'Actual runner required')
        with tempfile.TemporaryDirectory() as directory:
            base=Path(directory).resolve();binary=base/'physics_sim_job_runner';shutil.copyfile(selected,binary);binary.chmod(0o700)
            (base/'physics_sim_headless').write_bytes(b'non-executable fixture')
            request=base/'request.json';request.write_text(json.dumps({'schema_version':'physics_sim_headless_request_v1','runtime_scene_path':str(ROOT/'tests/fixtures/runtime_scene_primitive_retained.json'),'output_root':str(base/'output'),'grid':'8x8x8','frames':1,'sim_steps_per_frame':1,'skip_present':True}))
            jobs=base/'jobs';p=subprocess.run([str(binary),'submit','--request',str(request),'--jobs-root',str(jobs)],capture_output=True,text=True,timeout=5)
            self.assertNotEqual(p.returncode,0);self.assertIn('phase=3',p.stderr);self.assertIn('direct_child_reaped=true',p.stderr)
            slots=list(jobs.iterdir());self.assertEqual(len(slots),1);job=slots[0];status=json.loads((job/'job_status.json').read_text())
            self.assertEqual(status['state'],'failed');self.assertEqual(status['stage'],'startup_failed');self.assertGreater(status['pid'],0)
            self.assertIn('direct_child_reaped=true',status['diagnostics']);self.assertEqual((job/'stdout.log').read_bytes(),b'');self.assertEqual((job/'stderr.log').read_bytes(),b'');self.assertFalse((base/'output').exists())
if __name__=='__main__':unittest.main()
