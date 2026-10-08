"""Actual native persistence ownership/publication failures in disposable fixtures."""
import os
from pathlib import Path
import subprocess
import selectors
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]

class Persistence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        base = Path(cls.temp.name)
        harness = base/'probe.c'
        harness.write_text(r"""
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_persistence.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
int fault = 0;
int sync_count = 0;
int fail_fflush(FILE *s) { if (fault == 1) { errno=EIO; return EOF; } return fflush(s); }
int fail_fclose(FILE *s) { int v=fclose(s); if(fault==2){errno=EIO;return EOF;} return v; }
int fail_fsync(int fd) { if(fault==5 && ++sync_count==2){errno=EIO;return -1;} if(fault==3){errno=EIO;return -1;} return fsync(fd); }
int fail_renameat(int a,const char*b,int c,const char*d) {
    if(fault==4){errno=EIO;return -1;} return renameat(a,b,c,d);
}
int main(int argc,char **argv) {
    if(argc<3)return 90;
    if(!strcmp(argv[2],"runtime"))return physics_sim_persistence_runtime_directory()?0:1;
    PhysicsSimPersistence save;
    FILE *stream;
    if(!strcmp(argv[2],"tight"))stream=physics_sim_persistence_begin_bounded(argv[1],8,&save);
    else if(!strcmp(argv[2],"invalidlimit"))stream=physics_sim_persistence_begin_bounded(argv[1],0,&save);
    else if(!strcmp(argv[2],"excesslimit"))stream=physics_sim_persistence_begin_bounded(argv[1],UINT64_C(8)*1024*1024*1024+1,&save);
    else stream=physics_sim_persistence_begin(argv[1], &save);
    if(!stream)return 2;
    if(!strcmp(argv[2],"compete")) {
        PhysicsSimPersistence second; FILE *other=physics_sim_persistence_begin(argv[1],&second);
        if(other){physics_sim_persistence_finish(&second,other);return 91;}
    }
    if(!strcmp(argv[2],"swap")) {
        char old[1200];snprintf(old,sizeof(old),"%s.old",argv[1]);
        if(rename(argv[1],old))return 92;
        FILE *other=fopen(argv[1],"wb");if(!other)return 93;fputs("foreign",other);fclose(other);
    }
    if(!strcmp(argv[2],"lockswap")) {
        char lock[1200],old[1300];snprintf(lock,sizeof(lock),"%s/.physics-sim-persistence.lock",save.sidecar.parent);
        snprintf(old,sizeof(old),"%s.old",lock);if(rename(lock,old))return 94;
        FILE *other=fopen(lock,"wb");if(!other)return 95;fclose(other);
    }
    if(!strcmp(argv[2],"parent")) {
        char old[1200];snprintf(old,sizeof(old),"%s.old",save.sidecar.parent);
        if(rename(save.sidecar.parent,old)||mkdir(save.sidecar.parent,0700))return 96;
        FILE *other=fopen(argv[1],"wb");if(!other)return 97;fputs("foreign",other);fclose(other);
    }
    fputs("replacement",stream);
    if(!strcmp(argv[2],"pause")) { fflush(stream);puts("READY");fflush(stdout);for(;;)pause(); }
    if(!strcmp(argv[2],"flush"))fault=1;
    if(!strcmp(argv[2],"close"))fault=2;
    if(!strcmp(argv[2],"sync"))fault=3;
    if(!strcmp(argv[2],"rename"))fault=4;
    if(!strcmp(argv[2],"postrename"))fault=5;
    return physics_sim_persistence_finish(&save,stream)?0:3;
}
""")
        objects=[]
        for name in ('physics_sim_persistence','physics_sim_headless_output'):
            obj=base/(name+'.o');objects.append(str(obj))
            subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),
                '-Dfflush=fail_fflush','-Dfclose=fail_fclose','-Dfsync=fail_fsync','-Drenameat=fail_renameat',
                '-c',str(ROOT/'src/app'/f'{name}.c'),'-o',str(obj)],capture_output=True,check=True)
        cls.binary=base/'probe'
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(harness),*objects,'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.tempdir=tempfile.TemporaryDirectory();self.addCleanup(self.tempdir.cleanup)
        self.base=Path(self.tempdir.name);self.path=self.base/'preset.txt'
    def run_probe(self,mode='save',path=None,cwd=None):
        return subprocess.run([str(self.binary),str(path or self.path),mode],cwd=cwd,capture_output=True,text=True,timeout=5)
    def test_explicit_bound_holds_oversized_candidate_and_invalid_limits_allocate_nothing(self):
        for mode in ('invalidlimit','excesslimit'):
            self.assertEqual(self.run_probe(mode).returncode,2)
            self.assertEqual(list(self.base.iterdir()),[])
        self.path.write_text('original')
        self.assertEqual(self.run_probe('tight').returncode,3)
        self.assertEqual(self.path.read_text(),'original')
        self.assertEqual(len(list(self.base.glob('.headless-sidecar-*.pending'))),1)
    def test_create_replace_and_cooperative_competitor(self):
        self.assertEqual(self.run_probe().returncode,0);self.assertEqual(self.path.read_text(),'replacement')
        self.path.write_text('original')
        self.assertEqual(self.run_probe('compete').returncode,0)
        self.assertEqual(self.path.read_text(),'replacement')
    def test_flush_close_sync_rename_failures_preserve_predecessor_and_stage(self):
        for mode in ('flush','close','sync','rename'):
            with self.subTest(mode=mode):
                base=self.base/mode;base.mkdir();path=base/'preset';path.write_text('original')
                self.assertEqual(self.run_probe(mode,path).returncode,3)
                self.assertEqual(path.read_text(),'original')
                self.assertEqual(len(list(base.glob('.headless-sidecar-*.pending'))),1)
    def test_postrename_sync_failure_reports_unconfirmed_complete_bytes(self):
        self.path.write_text("original")
        self.assertEqual(self.run_probe("postrename").returncode,3)
        self.assertEqual(self.path.read_text(),"replacement")
        self.assertEqual(list(self.base.glob(".headless-sidecar-*.pending")),[])
    def test_failure_keeps_absent_destination_absent(self):
        self.assertEqual(self.run_probe('rename').returncode,3);self.assertFalse(self.path.exists())
        self.assertEqual(len(list(self.base.glob('.headless-sidecar-*.pending'))),1)
    def test_changed_predecessor_and_parent_preserve_foreign_bytes(self):
        for mode in ('swap','parent'):
            base=self.base/mode;base.mkdir();path=base/'preset';path.write_text('original')
            self.assertEqual(self.run_probe(mode,path).returncode,3)
            self.assertEqual(path.read_text(),'foreign')
    def test_interrupted_owner_preserves_predecessor_and_releases_kernel_lock(self):
        self.path.write_text('original')
        process=subprocess.Popen([str(self.binary),str(self.path),'pause'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            selector=selectors.DefaultSelector()
            try:
                selector.register(process.stdout,selectors.EVENT_READ)
                self.assertTrue(selector.select(5), 'native owner did not become ready')
                self.assertEqual(process.stdout.readline().strip(),'READY')
            finally:selector.close()
            self.assertEqual(self.run_probe().returncode,2)
            self.assertEqual(self.path.read_text(),'original')
            process.kill();process.wait(timeout=5)
            stages=list(self.base.glob('.headless-sidecar-*.pending'))
            self.assertEqual(len(stages),1)
            self.assertEqual(stages[0].read_text(),'replacement')
            self.assertEqual(self.run_probe().returncode,0)
            self.assertEqual(self.path.read_text(),'replacement')
            self.assertEqual(stages[0].read_text(),'replacement')
        finally:
            if process.poll() is None:process.kill();process.wait(timeout=5)
            process.stdout.close();process.stderr.close()
    def test_changed_lock_holds_predecessor(self):
        self.path.write_text('original');self.assertEqual(self.run_probe('lockswap').returncode,3)
        self.assertEqual(self.path.read_text(),'original')
    def test_symlink_hardlink_and_fifo_are_held(self):
        target=self.base/'target';target.write_text('original')
        linked=self.base/'linked';linked.symlink_to(target)
        hard=self.base/'hard';os.link(target,hard)
        fifo=self.base/'fifo';os.mkfifo(fifo)
        for path in (linked,hard,fifo):self.assertEqual(self.run_probe(path=path).returncode,2)
        self.assertEqual(target.read_text(),'original')
    def test_linked_or_nonempty_lock_is_held(self):
        self.path.write_text('original');lock=self.base/'.physics-sim-persistence.lock'
        lock.symlink_to(self.path);self.assertEqual(self.run_probe().returncode,2)
        lock.unlink();lock.write_text('foreign');self.assertEqual(self.run_probe().returncode,2)
        self.assertEqual(self.path.read_text(),'original');self.assertEqual(lock.read_text(),'foreign')
    def test_runtime_creation_and_linked_components(self):
        good=self.base/'good';good.mkdir()
        self.assertEqual(self.run_probe('runtime',cwd=good).returncode,0)
        self.assertTrue((good/'data/runtime').is_dir())
        foreign=self.base/'foreign';foreign.mkdir()
        bad=self.base/'bad';bad.mkdir();(bad/'data').symlink_to(foreign)
        self.assertNotEqual(self.run_probe('runtime',cwd=bad).returncode,0);self.assertEqual(list(foreign.iterdir()),[])
        nested=self.base/'nested';nested.mkdir();(nested/'data').mkdir();(nested/'data/runtime').symlink_to(foreign)
        self.assertNotEqual(self.run_probe('runtime',cwd=nested).returncode,0);self.assertEqual(list(foreign.iterdir()),[])
    def test_missing_parent_and_source_checkout_hold(self):
        self.assertEqual(self.run_probe(path=self.base/'missing/preset').returncode,2)
        self.assertFalse((self.base/'missing').exists())
        repo=self.base/'repo';repo.mkdir();(repo/'.git').write_text('marker');(repo/'src').mkdir()
        self.assertEqual(self.run_probe(path=repo/'src/preset').returncode,2)
        self.assertEqual(list((repo/'src').iterdir()),[])


class PreferenceSaves(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();base=Path(cls.temp.name)
        harness=base/'preferences.c'
        harness.write_text(r"""
#include "app/menu/shared_theme_font_adapter.h"
int main(void) {
    if(!physics_sim_shared_theme_set_preset("standard_grey") ||
       !physics_sim_shared_font_set_preset("ide"))return 9;
    return physics_sim_shared_theme_save_persisted() && physics_sim_shared_font_save_persisted()?0:2;
}
""")
        shared=ROOT/'third_party/codework_shared/core';cls.binary=base/'preferences'
        command=['clang','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-I/opt/homebrew/include',str(harness),str(ROOT/'src/app/menu/shared_theme_font_adapter.c'),str(ROOT/'src/app/physics_sim_persistence.c'),str(ROOT/'src/app/physics_sim_headless_output.c')]
        for module in ('core_theme','core_font','core_base'):
            command+=['-I'+str(shared/module/'include'),str(shared/module/'src'/f'{module}.c')]
        subprocess.run(command+['-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name)
    def invoke(self):return subprocess.run([str(self.binary)],cwd=self.base,capture_output=True,text=True,timeout=5)
    def test_actual_theme_font_save_create_and_replace(self):
        self.assertEqual(self.invoke().returncode,0)
        runtime=self.base/'data/runtime'
        theme=(runtime/'theme_preset.txt').read_bytes();font=(runtime/'font_preset.txt').read_bytes()
        self.assertTrue(theme.endswith(b'\n'));self.assertTrue(font.endswith(b'\n'))
        (runtime/'theme_preset.txt').write_text('old theme')
        (runtime/'font_preset.txt').write_text('old font')
        self.assertEqual(self.invoke().returncode,0)
        self.assertEqual((runtime/'theme_preset.txt').read_bytes(),theme)
        self.assertEqual((runtime/'font_preset.txt').read_bytes(),font)
    def test_actual_linked_theme_save_preserves_external_target(self):
        runtime=self.base/'data/runtime';runtime.mkdir(parents=True)
        foreign=self.base/'foreign';foreign.write_text('preserved')
        (runtime/'theme_preset.txt').symlink_to(foreign)
        self.assertEqual(self.invoke().returncode,2)
        self.assertEqual(foreign.read_text(),'preserved');self.assertFalse((runtime/'font_preset.txt').exists())
    def test_actual_linked_runtime_creates_no_external_files(self):
        foreign=self.base/'foreign';foreign.mkdir();(self.base/'data').symlink_to(foreign)
        self.assertEqual(self.invoke().returncode,2);self.assertEqual(list(foreign.iterdir()),[])

if __name__=='__main__':unittest.main()
