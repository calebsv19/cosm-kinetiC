"""Compiled startup graph admission; every fixture is disposable."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class RuntimeDirectories(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();base=Path(cls.temp.name)
        source=base/'probe.c'
        source.write_text(r"""
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/data_paths.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int fault=0;
int startup_mkdirat(int fd,const char*name,mode_t mode) {
    if(fault==1 && !strcmp(name,"runtime")){errno=EIO;return -1;}
    if(fault==2 && !strcmp(name,"runtime")) {
        if(rename("data","data.retained") || symlink("foreign","data"))return -1;
        fault=0;
    }
    return mkdirat(fd,name,mode);
}
int startup_fsync(int fd) {if(fault==3){errno=EIO;return -1;}return fsync(fd);}
int main(int argc,char **argv) {if(argc>1)fault=atoi(argv[1]);return physics_sim_ensure_runtime_dirs()?0:2;}
""")
        obj=base/'paths.o';cls.binary=base/'probe'
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-Dmkdirat=startup_mkdirat','-Dfsync=startup_fsync','-c',str(ROOT/'src/app/data_paths.c'),'-o',str(obj)],capture_output=True,check=True)
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),str(obj),'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name)
    def invoke(self,fault=0,cwd=None):return subprocess.run([str(self.binary),str(fault)],cwd=cwd or self.base,capture_output=True,text=True,timeout=5)
    def test_fresh_creation_and_existing_directory_reuse_preserve_files(self):
        self.assertEqual(self.invoke().returncode,0)
        paths=[self.base/'data',self.base/'data/runtime',self.base/'data/runtime/scenes',self.base/'data/snapshots']
        self.assertTrue(all(p.is_dir() for p in paths))
        sentinels=[]
        for p in paths:
            sentinel=p/'retained';sentinel.write_text('evidence');sentinels.append(sentinel)
        identities=[p.stat().st_ino for p in paths]
        self.assertEqual(self.invoke().returncode,0)
        self.assertEqual([p.stat().st_ino for p in paths],identities)
        self.assertTrue(all(p.read_text()=='evidence' for p in sentinels))
    def test_linked_data_and_runtime_never_modify_external_target(self):
        for slot in ('data','runtime'):
            base=self.base/slot;base.mkdir();foreign=base/'foreign';foreign.mkdir()
            if slot=='data':(base/'data').symlink_to(foreign)
            else:(base/'data').mkdir();(base/'data/runtime').symlink_to(foreign)
            self.assertEqual(self.invoke(cwd=base).returncode,2);self.assertEqual(list(foreign.iterdir()),[])
            self.assertFalse((base/'data/snapshots').exists())
    def test_late_invalid_snapshot_holds_before_missing_runtime_allocation(self):
        (self.base/'data').mkdir();foreign=self.base/'foreign';foreign.mkdir()
        (self.base/'data/snapshots').symlink_to(foreign)
        self.assertEqual(self.invoke().returncode,2)
        self.assertFalse((self.base/'data/runtime').exists());self.assertEqual(list(foreign.iterdir()),[])
    def test_invalid_scene_slot_holds_before_snapshot_allocation(self):
        runtime=self.base/'data/runtime';runtime.mkdir(parents=True);(runtime/'scenes').write_text('held')
        self.assertEqual(self.invoke().returncode,2);self.assertFalse((self.base/'data/snapshots').exists())
        self.assertEqual((runtime/'scenes').read_text(),'held')
    def test_regular_and_fifo_data_hold_without_blocking(self):
        for kind in ('regular','fifo'):
            base=self.base/kind;base.mkdir();data=base/'data'
            if kind=='regular':data.write_text('held')
            else:os.mkfifo(data)
            self.assertEqual(self.invoke(cwd=base).returncode,2)
            if kind=='regular':self.assertEqual(data.read_text(),'held')
    def test_partial_creation_failure_retains_and_next_call_reuses(self):
        self.assertEqual(self.invoke(1).returncode,2)
        self.assertTrue((self.base/'data').is_dir());self.assertFalse((self.base/'data/runtime').exists())
        inode=(self.base/'data').stat().st_ino
        self.assertEqual(self.invoke().returncode,0);self.assertEqual((self.base/'data').stat().st_ino,inode)
    def test_sync_failure_is_not_success_and_keeps_created_directory(self):
        self.assertEqual(self.invoke(3).returncode,2);self.assertTrue((self.base/'data').is_dir())
        self.assertEqual(self.invoke().returncode,0)
    def test_swapped_parent_is_not_followed_and_is_reported_held(self):
        (self.base/'data').mkdir();foreign=self.base/'foreign';foreign.mkdir()
        self.assertEqual(self.invoke(2).returncode,2)
        self.assertEqual(list(foreign.iterdir()),[])
        self.assertTrue((self.base/'data.retained').is_dir())
    def test_checkout_root_allowed_source_subdirectory_held(self):
        (self.base/'.git').write_text('fixture');source=self.base/'src';source.mkdir()
        self.assertEqual(self.invoke(cwd=source).returncode,2);self.assertEqual(list(source.iterdir()),[])
        self.assertEqual(self.invoke().returncode,0)
    def test_home_root_and_system_storage_held(self):
        env=os.environ.copy();env['HOME']=str(self.base)
        result=subprocess.run([str(self.binary)],cwd=self.base,env=env,capture_output=True,timeout=5)
        self.assertEqual(result.returncode,2);self.assertEqual(list(self.base.iterdir()),[])
        self.assertEqual(self.invoke(cwd='/usr').returncode,2)
if __name__=='__main__':unittest.main()
