"""Exclusive log slots and descriptor-only native worker handoff."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class LogHandoff(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t=tempfile.TemporaryDirectory();base=Path(cls.t.name);cls.binary=base/'probe';source=base/'probe.c'
        source.write_text(r"""#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_logs.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
int main(int argc,char **argv){if(argc!=3)return 3;PhysicsSimJobLogs logs;
if(strcmp(argv[2],"closed")==0){close(0);close(1);close(2);}
if(!physics_sim_job_logs_prepare(argv[1],&logs))return 4;
if(strcmp(argv[2],"root")==0){char old[1200];snprintf(old,sizeof(old),"%s.old",argv[1]);rename(argv[1],old);mkdir(argv[1],0700);}
if(strcmp(argv[2],"replace")==0){char path[1200],old[1200];snprintf(path,sizeof(path),"%s/stdout.log",argv[1]);snprintf(old,sizeof(old),"%s/old.log",argv[1]);rename(path,old);FILE *f=fopen(path,"wb");fputs("foreign",f);fclose(f);}
if(strcmp(argv[2],"inplace")==0){char path[1200];snprintf(path,sizeof(path),"%s/stderr.log",argv[1]);FILE *f=fopen(path,"wb");fputs("foreign",f);fclose(f);}
if(strcmp(argv[2],"close")==0){physics_sim_job_logs_close(&logs);return logs.stdout_descriptor==-1&&logs.stderr_descriptor==-1&&logs.root_descriptor==-1?0:5;}
int changed=strcmp(argv[2],"root")==0||strcmp(argv[2],"replace")==0||strcmp(argv[2],"inplace")==0;
if(!physics_sim_job_logs_redirect(&logs)){physics_sim_job_logs_close(&logs);return changed?0:6;}
if(changed)return 7;
execl("/bin/sh","sh","-c","printf 'OUT'; printf 'ERR' >&2; if read line; then exit 8; else printf 'EOF'; fi",(char*)0);return 9;}
""")
        flags=shlex.split(subprocess.check_output([os.environ.get('PKG_CONFIG','pkg-config'),'--cflags','--libs','json-c'],text=True))
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),*[str(ROOT/'src/app'/n) for n in ['physics_sim_job_logs.c','physics_sim_job_guard.c','physics_sim_headless_output.c']],*flags,'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.t.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name).resolve();self.root=self.base/'job';self.root.mkdir()
    def invoke(self,mode='exec',root=None):return subprocess.run([str(self.binary),str(root or self.root),mode],capture_output=True,timeout=5)
    def test_exec_receives_exact_logs_and_null_stdin(self):
        p=self.invoke();self.assertEqual(p.returncode,0,p.stderr);self.assertEqual(p.stdout,b'');self.assertEqual((self.root/'stdout.log').read_bytes(),b'OUTEOF');self.assertEqual((self.root/'stderr.log').read_bytes(),b'ERR')
    def test_closed_standard_streams_do_not_alias_log_descriptors(self):
        self.assertEqual(self.invoke('closed').returncode,0);self.assertEqual((self.root/'stdout.log').read_bytes(),b'OUTEOF');self.assertEqual((self.root/'stderr.log').read_bytes(),b'ERR')
    def test_existing_second_slot_holds_before_any_new_log(self):
        (self.root/'stderr.log').write_bytes(b'previous');self.assertEqual(self.invoke().returncode,4);self.assertFalse((self.root/'stdout.log').exists());self.assertEqual((self.root/'stderr.log').read_bytes(),b'previous')
    def test_links_special_files_and_hardlinks_never_receive_output(self):
        foreign=self.base/'foreign';foreign.write_bytes(b'keep');slot=self.root/'stderr.log'
        slot.symlink_to(foreign);self.assertEqual(self.invoke().returncode,4);slot.unlink()
        os.mkfifo(slot);self.assertEqual(self.invoke().returncode,4);slot.unlink()
        os.link(foreign,slot);self.assertEqual(self.invoke().returncode,4);self.assertEqual(foreign.read_bytes(),b'keep');self.assertFalse((self.root/'stdout.log').exists())
    def test_replaced_root_holds_child_handoff(self):
        self.assertEqual(self.invoke('root').returncode,0);self.assertEqual(list(self.root.iterdir()),[]);self.assertEqual((self.base/'job.old/stdout.log').read_bytes(),b'')
    def test_replaced_log_holds_and_preserves_foreign_bytes(self):
        self.assertEqual(self.invoke('replace').returncode,0);self.assertEqual((self.root/'stdout.log').read_bytes(),b'foreign');self.assertEqual((self.root/'old.log').read_bytes(),b'')
    def test_inplace_log_change_holds_handoff(self):
        self.assertEqual(self.invoke('inplace').returncode,0);self.assertEqual((self.root/'stderr.log').read_bytes(),b'foreign');self.assertEqual((self.root/'stdout.log').read_bytes(),b'')
    def test_linked_and_protected_roots_are_held(self):
        link=self.base/'link';link.symlink_to(self.root);self.assertEqual(self.invoke(root=link).returncode,4)
        repo=self.base/'repo';repo.mkdir();(repo/'.git').write_text('fixture');self.assertEqual(self.invoke(root=repo/'src/logs').returncode,4);self.assertFalse((repo/'src').exists());self.assertEqual(list(self.root.iterdir()),[])
    def test_close_releases_descriptors_without_removing_log_evidence(self):
        self.assertEqual(self.invoke('close').returncode,0);self.assertEqual(sorted(p.name for p in self.root.iterdir()),['stderr.log','stdout.log'])
if __name__=='__main__':unittest.main()
