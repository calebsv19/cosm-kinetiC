"""Real native metadata staging, competing writers and interrupted publication."""
import json
import os
from pathlib import Path
import selectors
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AtomicJobs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t=tempfile.TemporaryDirectory();base=Path(cls.t.name);cls.binary=base/'probe'
        source=base/'probe.c'
        source.write_text(r"""#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_file.h"
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <signal.h>
int main(int argc,char **argv){if(argc!=3)return 3;PhysicsSimJobFile file;
FILE *stream=physics_sim_job_file_begin(argv[1],strcmp(argv[2],"text")!=0,&file);if(!stream)return 4;
if(strcmp(argv[2],"text")==0)fputs("1234\n",stream);
else if(strcmp(argv[2],"invalid")==0)fputs("{\"v\":NaN}",stream);
else if(strcmp(argv[2],"pause")==0){fputs("{\"v\":",stream);fflush(stream);fsync(fileno(stream));puts("READY");fflush(stdout);sleep(30);}
else if(strcmp(argv[2],"repeat")==0){fputs("{\"v\":2}",stream);if(!physics_sim_job_file_finish(&file,stream))return 5;puts("READY");fflush(stdout);for(int n=3;n<=80;n++){stream=physics_sim_job_file_begin(argv[1],1,&file);if(!stream)return 6;fprintf(stream,"{\"v\":%d}",n);if(!physics_sim_job_file_finish(&file,stream))return 7;}return 0;}
else fputs("{\"v\":2}",stream);
if(strcmp(argv[2],"pair-invalid")==0){
 if(!physics_sim_job_file_prepare(&file,stream))return 12;
 char parent[1200],other_path[1300];snprintf(parent,sizeof(parent),"%s/reports",file.sidecar.parent);snprintf(other_path,sizeof(other_path),"%s/report.json",parent);
 PhysicsSimJobFile other;FILE *second=physics_sim_job_file_begin(other_path,1,&other);
 if(!second){physics_sim_job_file_discard(&file,stream);return 14;}
 fputs("{\"v\":NaN}",second);int unexpected=physics_sim_job_file_prepare(&other,second);
 if(unexpected)physics_sim_job_file_discard(&other,second);
 physics_sim_job_file_discard(&file,stream);return unexpected?15:0;
}
if(strncmp(argv[2],"prepared-",9)==0){
 if(!physics_sim_job_file_prepare(&file,stream))return 12;
 if(strcmp(argv[2],"prepared-pause")==0){puts("READY");fflush(stdout);sleep(30);}
 if(strcmp(argv[2],"prepared-discard")==0){physics_sim_job_file_discard(&file,stream);return 0;}
 if(strcmp(argv[2],"prepared-buffer")==0)fputs(" ",stream);
 if(strcmp(argv[2],"prepared-rewrite")==0){char path[1200];snprintf(path,sizeof(path),"%s/%s",file.sidecar.parent,file.sidecar.pending_name);FILE *other=fopen(path,"wb");fputs("{\"v\":2}",other);fclose(other);}
 if(strcmp(argv[2],"prepared-replace")==0){char path[1200],old[1300];snprintf(path,sizeof(path),"%s/%s",file.sidecar.parent,file.sidecar.pending_name);snprintf(old,sizeof(old),"%s.old",path);rename(path,old);FILE *other=fopen(path,"wb");fputs("{\"v\":2}",other);fclose(other);}
 if(strcmp(argv[2],"prepared-predecessor")==0){FILE *other=fopen(argv[1],"wb");fputs("{\"v\":9}",other);fclose(other);}
 return physics_sim_job_file_publish(&file,stream)?0:13;
}
if(strcmp(argv[2],"replace")==0){char old[1200];snprintf(old,sizeof(old),"%s.old",argv[1]);rename(argv[1],old);FILE *other=fopen(argv[1],"wb");fputs("foreign",other);fclose(other);}
if(strcmp(argv[2],"inplace")==0){FILE *other=fopen(argv[1],"wb");fputs("{\"v\":9}",other);fclose(other);}
if(strcmp(argv[2],"lock-replace")==0){char lock[1200],old[1200];snprintf(lock,sizeof(lock),"%s/.physics-sim-job-metadata.lock",file.sidecar.parent);snprintf(old,sizeof(old),"%s/.old-lock",file.sidecar.parent);rename(lock,old);FILE *other=fopen(lock,"wb");fputs("foreign",other);fclose(other);}
if(strcmp(argv[2],"io-error")==0){struct rlimit limit;if(getrlimit(RLIMIT_FSIZE,&limit))return 8;limit.rlim_cur=16;signal(SIGXFSZ,SIG_IGN);if(setrlimit(RLIMIT_FSIZE,&limit))return 9;setvbuf(stream,NULL,_IONBF,0);fputs("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",stream);if(!ferror(stream))return 10;}
return physics_sim_job_file_finish(&file,stream)?0:11;}
""")
        flags=shlex.split(subprocess.check_output([os.environ.get('PKG_CONFIG','pkg-config'),'--cflags','--libs','json-c'],text=True))
        flags.extend(['-I'+str(ROOT/'third_party/codework_shared/core/core_scene_compile/include'),'-I'+str(ROOT/'third_party/codework_shared/core/core_base/include'),str(ROOT/'third_party/codework_shared/core/core_scene_compile/src/core_scene_compile_digest.c')])
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),*[str(ROOT/'src/app'/n) for n in ['physics_sim_job_file.c','physics_sim_job_guard.c','physics_sim_job_json.c','physics_sim_headless_output.c']],*flags,'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.t.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name).resolve();self.path=self.base/'status.json'
    def invoke(self,mode,path=None):return subprocess.run([str(self.binary),str(path or self.path),mode],capture_output=True,text=True,timeout=5)
    def predecessor(self):self.path.write_text('{"v":1}')
    def pending(self):return list(self.base.glob('.headless-sidecar-*.pending'))
    def child(self,mode):
        p=subprocess.Popen([str(self.binary),str(self.path),mode],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        def cleanup():
            if p.poll() is None:p.kill();p.wait(timeout=5)
            p.stdout.close();p.stderr.close()
        self.addCleanup(cleanup)
        selector=selectors.DefaultSelector();selector.register(p.stdout,selectors.EVENT_READ)
        try:self.assertTrue(selector.select(5),'No stage readiness')
        finally:selector.close()
        self.assertEqual(p.stdout.readline().strip(),'READY');return p
    def test_initial_destination_remains_absent_after_forced_death(self):
        p=self.child('pause');self.assertFalse(self.path.exists());self.assertEqual(len(self.pending()),1)
        p.kill();p.wait(timeout=5);self.assertFalse(self.path.exists());self.assertEqual(self.pending()[0].read_bytes(),b'{"v":')
    def test_predecessor_survives_forced_death_and_next_writer_can_proceed(self):
        self.predecessor();p=self.child('pause');p.kill();p.wait(timeout=5)
        self.assertEqual(json.loads(self.path.read_text()),{'v':1});self.assertEqual(self.invoke('normal').returncode,0)
        self.assertEqual(json.loads(self.path.read_text()),{'v':2});self.assertEqual(len(self.pending()),1)
    def test_competing_writer_is_held_without_another_stage(self):
        self.predecessor();p=self.child('pause');result=self.invoke('normal')
        self.assertEqual(result.returncode,4,result.stderr);self.assertEqual(len(self.pending()),1);self.assertEqual(json.loads(self.path.read_text()),{'v':1})
    def test_atomic_readers_observe_complete_generations(self):
        self.predecessor();p=self.child('repeat');observed=[]
        while p.poll() is None:observed.append(json.loads(self.path.read_text())['v'])
        self.assertEqual(p.wait(timeout=5),0,p.stderr.read());self.assertEqual(json.loads(self.path.read_text()),{'v':80})
        self.assertTrue(all(1<=v<=80 for v in observed));self.assertEqual(self.pending(),[])
    def test_invalid_payload_is_held(self):
        self.predecessor();self.assertEqual(self.invoke('invalid').returncode,11);self.assertEqual(json.loads(self.path.read_text()),{'v':1});self.assertEqual(len(self.pending()),1)
    def test_inplace_edit_holds_predecessor_publication(self):
        self.predecessor();self.assertEqual(self.invoke('inplace').returncode,11);self.assertEqual(json.loads(self.path.read_text()),{'v':9});self.assertEqual(len(self.pending()),1)
    def test_foreign_replacement_is_preserved(self):
        self.predecessor();self.assertEqual(self.invoke('replace').returncode,11);self.assertEqual(self.path.read_bytes(),b'foreign');self.assertEqual((self.base/'status.json.old').read_text(),'{"v":1}');self.assertEqual(len(self.pending()),1)
    def test_replaced_lock_holds_publication(self):
        self.predecessor();self.assertEqual(self.invoke('lock-replace').returncode,11);self.assertEqual(json.loads(self.path.read_text()),{'v':1});self.assertEqual((self.base/'.physics-sim-job-metadata.lock').read_bytes(),b'foreign');self.assertEqual(len(self.pending()),1)
    def test_io_failure_preserves_previous_generation(self):
        self.predecessor();self.assertEqual(self.invoke('io-error').returncode,11);self.assertEqual(json.loads(self.path.read_text()),{'v':1});self.assertEqual(len(self.pending()),1)
    def test_unknown_metadata_and_linked_lock_are_held_before_staging(self):
        self.path.write_bytes(b'invalid');self.assertEqual(self.invoke('normal').returncode,4);self.assertEqual(self.pending(),[])
        self.predecessor();other=self.base/'foreign';other.write_bytes(b'keep');(self.base/'.physics-sim-job-metadata.lock').symlink_to(other)
        self.assertEqual(self.invoke('normal').returncode,4);self.assertEqual(other.read_bytes(),b'keep');self.assertEqual(self.pending(),[])
    def test_text_publication_is_bounded_and_preserves_external_links(self):
        self.assertEqual(self.invoke('text').returncode,0);self.assertEqual(self.path.read_bytes(),b'1234\n')
        self.path.unlink();other=self.base/'foreign';other.write_bytes(b'keep');self.path.symlink_to(other)
        self.assertEqual(self.invoke('text').returncode,4);self.assertEqual(other.read_bytes(),b'keep')
    def test_second_stage_rejection_preserves_both_predecessors(self):
        self.predecessor();parent=self.base/'reports';parent.mkdir();report=parent/'report.json';report.write_bytes(b'{"v":7}')
        self.assertEqual(self.invoke('pair-invalid').returncode,0)
        self.assertEqual(self.path.read_bytes(),b'{"v":1}');self.assertEqual(report.read_bytes(),b'{"v":7}')
        self.assertEqual(self.pending()[0].read_bytes(),b'{"v":2}')
        self.assertEqual(list(parent.glob('.headless-sidecar-*.pending'))[0].read_bytes(),b'{"v":NaN}')
        self.assertEqual(self.invoke('normal').returncode,0)
        self.assertEqual(self.invoke('normal',report).returncode,0)
    def test_prepared_stage_survives_death_without_destination_mutation(self):
        self.predecessor();p=self.child('prepared-pause')
        self.assertEqual(self.path.read_bytes(),b'{"v":1}')
        self.assertEqual(self.pending()[0].read_bytes(),b'{"v":2}')
        self.assertEqual(self.invoke('normal').returncode,4)
        p.kill();p.wait(timeout=5)
        self.assertEqual(self.path.read_bytes(),b'{"v":1}')
        self.assertEqual(self.pending()[0].read_bytes(),b'{"v":2}')
    def test_prepared_discard_retains_stage_and_releases_lock(self):
        for present in (False, True):
            with self.subTest(present=present):
                if present:self.predecessor()
                self.assertEqual(self.invoke('prepared-discard').returncode,0)
                self.assertEqual(self.path.exists(),present)
                if present:self.assertEqual(self.path.read_bytes(),b'{"v":1}')
                self.assertTrue(all(p.read_bytes()==b'{"v":2}' for p in self.pending()))
                self.assertEqual(self.invoke('normal').returncode,0)
                self.path.unlink()
    def test_prepared_publication_rejects_buffered_and_same_byte_stage_changes(self):
        for mode in ('prepared-buffer','prepared-rewrite','prepared-replace'):
            with self.subTest(mode=mode):
                self.predecessor();self.assertEqual(self.invoke(mode).returncode,13)
                self.assertEqual(self.path.read_bytes(),b'{"v":1}')
                self.assertTrue(self.pending())
    def test_prepared_publication_rechecks_predecessor(self):
        self.predecessor();self.assertEqual(self.invoke('prepared-predecessor').returncode,13)
        self.assertEqual(self.path.read_bytes(),b'{"v":9}')
        self.assertEqual(self.pending()[0].read_bytes(),b'{"v":2}')
    def test_prepared_publication_publishes_complete_initial_and_replacement(self):
        self.assertEqual(self.invoke('prepared-normal').returncode,0)
        self.assertEqual(self.path.read_bytes(),b'{"v":2}');self.assertEqual(self.pending(),[])
        self.predecessor();self.assertEqual(self.invoke('prepared-normal').returncode,0)
        self.assertEqual(self.path.read_bytes(),b'{"v":2}');self.assertEqual(self.pending(),[])
if __name__=='__main__':unittest.main()
