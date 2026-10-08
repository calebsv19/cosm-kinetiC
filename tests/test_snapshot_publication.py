"""Actual 2D snapshot binary bytes and retained publication failures."""
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Snapshots(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();base=Path(cls.temp.name);source=base/'probe.c'
        source.write_text(r"""
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_backend_2d_internal.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int fault=0;
size_t snapshot_fwrite(const void*p,size_t size,size_t n,FILE*f){if(fault==1 && n>1){fault=0;return fwrite(p,size,n/2,f);}return fwrite(p,size,n,f);}
int snapshot_fflush(FILE*f){if(fault==2){errno=EIO;return EOF;}return fflush(f);}
int snapshot_fclose(FILE*f){int v=fclose(f);if(fault==3){errno=EIO;return EOF;}return v;}
int snapshot_renameat(int a,const char*b,int c,const char*d){if(fault==4){errno=EIO;return -1;}return renameat(a,b,c,d);}
int main(int argc,char**argv){if(argc<3)return 9;AppConfig cfg={0};FluidScenePreset preset={0};cfg.grid_w=4;cfg.grid_h=3;cfg.window_w=100;cfg.window_h=100;
SimRuntimeBackend*backend=sim_runtime_backend_create(&cfg,&preset,NULL,NULL);if(!backend)return 8;
SimRuntimeBackend2D *state=backend_2d_state(backend);int w=state->fluid->w,h=state->fluid->h;
size_t count=(size_t)w*h;for(size_t i=0;i<count;i++){state->fluid->density[i]=(float)i+1;state->fluid->velX[i]=-(float)i;state->fluid->velY[i]=(float)i*0.5f;}
fault=atoi(argv[2]);double time=0.125;if(fault==5){state->fluid->w=INT_MAX;state->fluid->h=INT_MAX;}if(fault==6)time=NAN;
int ok=sim_runtime_backend_export_snapshot(backend,time,argv[1]);state->fluid->w=w;state->fluid->h=h;sim_runtime_backend_destroy(backend);return ok?0:2;}
""")
        include=os.environ.get('FISICS_INCLUDE_DIR','/Users/calebsv/Desktop/CodeWork/fisiCs/include')
        flags=['clang','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-I'+str(ROOT/'src'),'-I'+include]
        flags += ['-I'+str(path) for path in sorted((ROOT/'third_party/codework_shared/core').glob('*/include'))]
        flags += ['-I'+str(ROOT/'third_party/codework_shared/shape'),'-I'+str(ROOT/'third_party/codework_shared/shape/external'),'-I/opt/homebrew/include','-I'+str(ROOT/'third_party/codework_shared/timer_hud/include'),'-I'+str(ROOT/'src/tools')]
        sources=['src/app/sim_runtime_backend.c','src/app/sim_runtime_backend_2d.c','src/app/sim_runtime_backend_2d_runtime_fields.c','src/app/atmospheric/atmospheric_field.c','src/physics/fluid2d/fluid2d.c','src/physics/fluid2d/fluid2d_boundary.c','src/app/physics_sim_persistence.c','src/app/physics_sim_headless_output.c','tests/sim_runtime_backend_2d_contract_test.c']
        objects=[]
        for index,rel in enumerate(sources):
            obj=base/f'{index}.o';objects.append(str(obj));defines=[]
            if rel=='tests/sim_runtime_backend_2d_contract_test.c':defines=['-Dmain=unused_contract_main']
            elif rel in ('src/app/sim_runtime_backend_2d.c','src/app/physics_sim_persistence.c','src/app/physics_sim_headless_output.c'):defines=['-Dfwrite=snapshot_fwrite','-Dfflush=snapshot_fflush','-Dfclose=snapshot_fclose','-Drenameat=snapshot_renameat']
            result=subprocess.run([*flags,*defines,'-c',str(ROOT/rel),'-o',str(obj)],capture_output=True,text=True)
            if result.returncode:raise RuntimeError(result.stderr)
        cls.flags=flags;cls.objects=objects;cls.sources=sources;cls.base=base
        cls.binary=base/'probe';subprocess.run([*flags,str(source),*objects,'-lm','-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name);self.path=self.base/'snapshot.bin'
    def invoke(self,fault=0,path=None):return subprocess.run([str(self.binary),str(path or self.path),str(fault)],capture_output=True,text=True,timeout=5)
    def expected(self):
        return struct.pack('=IIIId',int.from_bytes(b'PS2D','big'),1,4,3,0.125)+struct.pack('=12f',*range(1,13))+struct.pack('=12f',*[-float(i) for i in range(12)])+struct.pack('=12f',*[i*0.5 for i in range(12)])
    def test_exact_original_format_create_and_replace(self):
        self.assertEqual(self.invoke().returncode,0);self.assertEqual(self.path.read_bytes(),self.expected())
        self.path.write_bytes(b'previous');self.assertEqual(self.invoke().returncode,0);self.assertEqual(self.path.read_bytes(),self.expected())
    def test_partial_write_flush_close_and_rename_faults_preserve_predecessor(self):
        for fault in (1,2,3,4):
            base=self.base/str(fault);base.mkdir();path=base/'snapshot.bin';path.write_bytes(b'previous')
            self.assertEqual(self.invoke(fault,path).returncode,2);self.assertEqual(path.read_bytes(),b'previous')
            stages=list(base.glob('.headless-sidecar-*.pending'));self.assertEqual(len(stages),1)
            if fault==1:self.assertGreater(stages[0].stat().st_size,24);self.assertLess(stages[0].stat().st_size,len(self.expected()))
    def test_failed_first_snapshot_keeps_final_absent(self):
        self.assertEqual(self.invoke(1).returncode,2);self.assertFalse(self.path.exists())
        self.assertEqual(len(list(self.base.glob('.headless-sidecar-*.pending'))),1)
    def test_bad_shape_and_nonfinite_time_hold_before_staging(self):
        self.path.write_bytes(b'previous')
        for fault in (5,6):self.assertEqual(self.invoke(fault).returncode,2)
        self.assertEqual(self.path.read_bytes(),b'previous');self.assertEqual(list(self.base.glob('.headless-sidecar-*.pending')),[])
    def test_linked_and_special_destination_preserve_external_target(self):
        target=self.base/'target';target.write_bytes(b'previous');self.path.symlink_to(target)
        self.assertEqual(self.invoke().returncode,2);self.assertEqual(target.read_bytes(),b'previous')
        self.path.unlink();os.mkfifo(self.path);self.assertEqual(self.invoke().returncode,2)
    def test_large_predecessor_uses_snapshot_bound_not_metadata_bound(self):
        with self.path.open('wb') as f:f.truncate(20*1024*1024)
        self.assertEqual(self.invoke().returncode,0);self.assertEqual(self.path.read_bytes(),self.expected())
    def test_missing_or_linked_parent_and_competing_lock_are_held(self):
        self.assertEqual(self.invoke(path=self.base/'missing/snapshot').returncode,2);self.assertFalse((self.base/'missing').exists())
        linked=self.base/'linked';linked.symlink_to(self.base)
        self.assertEqual(self.invoke(path=linked/'snapshot.bin').returncode,2)
        lock=self.base/'.physics-sim-persistence.lock';lock.write_text('foreign')
        self.assertEqual(self.invoke().returncode,2);self.assertEqual(lock.read_text(),'foreign')
if __name__=='__main__':unittest.main()
