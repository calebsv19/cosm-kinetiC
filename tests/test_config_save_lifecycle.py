"""Native configuration serialization and publication in disposable roots."""
import json
import os
from pathlib import Path
import subprocess
import shlex
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class ConfigSaves(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();base=Path(cls.temp.name)
        source=base/'probe.c'
        source.write_text(r"""
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "config/config_loader.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int fault=0;
int config_fail_fflush(FILE *s){if(fault==1){errno=EIO;return EOF;}return fflush(s);}
int config_fail_fclose(FILE *s){int v=fclose(s);if(fault==2){errno=EIO;return EOF;}return v;}
int config_fail_renameat(int a,const char*b,int c,const char*d){if(fault==3){errno=EIO;return -1;}return renameat(a,b,c,d);}
int main(int argc,char **argv){if(argc<3)return 9;AppConfig cfg=app_config_default();cfg.grid_w=17;cfg.grid_h=19;
fault=atoi(argv[2]);if(!config_loader_save(&cfg,argv[1]))return 2;
AppConfig read={0};ConfigLoadOptions opts={.path=argv[1],.allow_missing=false};
if(!config_loader_load(&read,&opts)||read.grid_w!=17||read.grid_h!=19)return 3;return 0;}
""")
        objects=[]
        for rel in ('src/config/config_loader.c','src/app/physics_sim_job_json.c','src/app/physics_sim_persistence.c','src/app/physics_sim_headless_output.c','src/app/app_config.c','src/app/data_paths.c'):
            obj=base/(Path(rel).stem+'.o');objects.append(str(obj))
            command=['clang','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-I'+str(ROOT/'src'),'-Dfflush=config_fail_fflush','-Dfclose=config_fail_fclose','-Drenameat=config_fail_renameat','-c',str(ROOT/rel),'-o',str(obj)]
            command += shlex.split(subprocess.check_output(['pkg-config','--cflags','json-c'],text=True))
            subprocess.run(command,capture_output=True,check=True)
        cls.binary=base/'probe'
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),*objects,*shlex.split(subprocess.check_output(['pkg-config','--libs','json-c'],text=True)),'-lm','-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.tempdir=tempfile.TemporaryDirectory();self.addCleanup(self.tempdir.cleanup);self.base=Path(self.tempdir.name);self.path=self.base/'config.json'
    def invoke(self,fault=0,path=None):return subprocess.run([str(self.binary),str(path or self.path),str(fault)],capture_output=True,text=True,timeout=5)
    def test_create_replace_and_parse_actual_saved_config(self):
        self.assertEqual(self.invoke().returncode,0)
        saved=json.loads(self.path.read_text());self.assertEqual(saved['grid']['width'],17)
        self.path.write_text('original');self.assertEqual(self.invoke().returncode,0)
        self.assertEqual(json.loads(self.path.read_text()),saved)
    def test_flush_close_rename_failures_keep_previous_bytes_and_attempts(self):
        for fault in (1,2,3):
            base=self.base/str(fault);base.mkdir();path=base/'config.json';path.write_text('original')
            self.assertEqual(self.invoke(fault,path).returncode,2)
            self.assertEqual(path.read_text(),'original')
            self.assertEqual(len(list(base.glob('.headless-sidecar-*.pending'))),1)
    def test_failed_first_save_does_not_publish_partial_config(self):
        self.assertEqual(self.invoke(3).returncode,2);self.assertFalse(self.path.exists())
    def test_linked_destination_and_parent_preserve_external_storage(self):
        foreign=self.base/'foreign';foreign.mkdir();target=foreign/'config';target.write_text('original')
        self.path.symlink_to(target);self.assertEqual(self.invoke().returncode,2)
        linked=self.base/'linked';linked.symlink_to(foreign)
        self.assertEqual(self.invoke(path=linked/'new').returncode,2)
        self.assertEqual(target.read_text(),'original');self.assertEqual(list(foreign.iterdir()),[target])
    def test_existing_source_checkout_config_is_held(self):
        repo=self.base/'repo';repo.mkdir();(repo/'.git').write_text('marker');source=repo/'config';source.mkdir();path=source/'app.json';path.write_text('original')
        self.assertEqual(self.invoke(path=path).returncode,2);self.assertEqual(path.read_text(),'original')
    def test_fifo_and_nonempty_lock_hold_without_blocking(self):
        os.mkfifo(self.path);self.assertEqual(self.invoke().returncode,2)
        self.path.unlink();self.path.write_text('original')
        lock=self.base/'.physics-sim-persistence.lock';lock.write_text('foreign')
        self.assertEqual(self.invoke().returncode,2);self.assertEqual(self.path.read_text(),'original');self.assertEqual(lock.read_text(),'foreign')
if __name__=='__main__':unittest.main()
