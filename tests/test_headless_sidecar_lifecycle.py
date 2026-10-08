"""Compiled sidecar preflight and descriptor replacement safety."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Sidecars(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();base=Path(cls.temp.name);cls.bin=base/'probe'
        harness=base/'probe.c'
        harness.write_text(r'''#include "app/physics_sim_headless_output.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc,char **argv){char path[1024],error[256];if(argc<4)return 3;
if(!physics_sim_headless_sidecar_plan(argv[1],argc>4?argv[4]:NULL,strcmp(argv[2],"-")==0?NULL:argv[2],"run_summary.json",true,path,sizeof(path),error,sizeof(error))){puts(error);return 2;}
if(strcmp(argv[3],"plan")==0){puts(path);return 0;}
PhysicsSimHeadlessSidecar sidecar;if(!physics_sim_headless_sidecar_claim(path,&sidecar))return 4;
if(strcmp(argv[3],"replace")==0){char old[1100];snprintf(old,sizeof(old),"%s.old",path);if(rename(path,old))return 5;FILE *foreign=fopen(path,"wb");fputs("foreign",foreign);fclose(foreign);}
if(strcmp(argv[3],"parent")==0){char old[1100];snprintf(old,sizeof(old),"%s.old",sidecar.parent);if(rename(sidecar.parent,old)||mkdir(sidecar.parent,0700))return 5;FILE *foreign=fopen(path,"wb");fputs("foreign",foreign);fclose(foreign);}
FILE *stream=physics_sim_headless_sidecar_stream(&sidecar);if(!stream){physics_sim_headless_sidecar_close(&sidecar);return 6;}
fputs("owned",stream);int ok=physics_sim_headless_sidecar_publish(&sidecar,stream);physics_sim_headless_sidecar_close(&sidecar);return ok?0:7;}
''')
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(harness),str(ROOT/'src/app/physics_sim_headless_output.c'),'-o',str(cls.bin)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name);self.out=self.base/'out';self.out.mkdir()
    def invoke(self,selected='-',mode='plan',input=None):
        return subprocess.run([str(self.bin),str(self.out),str(selected),mode,*([str(input)] if input else [])],capture_output=True,text=True,timeout=5)
    def test_default_and_fresh_external_write(self):
        self.assertEqual(self.invoke(mode='write').returncode,0);self.assertEqual((self.out/'run_summary.json').read_text(),'owned')
        external=self.base/'external.json';self.assertEqual(self.invoke(external,'write').returncode,0);self.assertEqual(external.read_text(),'owned')
    def test_existing_external_file_is_held(self):
        path=self.base/'existing';path.write_bytes(b'preserved')
        self.assertNotEqual(self.invoke(path,'write').returncode,0);self.assertEqual(path.read_bytes(),b'preserved')
    def test_file_and_parent_replacement_preserves_foreign_bytes(self):
        for mode in ('replace','parent'):
            parent=self.base/mode;parent.mkdir();path=parent/'sidecar.json'
            result=self.invoke(path,mode);self.assertEqual(result.returncode,6,result.stdout+result.stderr)
            self.assertEqual(path.read_bytes(),b'foreign')
    def test_linked_and_special_leaves_are_held(self):
        target=self.base/'target';target.write_bytes(b'held');link=self.out/'linked';link.symlink_to(target)
        fifo=self.out/'fifo';os.mkfifo(fifo)
        for path in (link,fifo):self.assertNotEqual(self.invoke(path,'write').returncode,0)
        self.assertEqual(target.read_bytes(),b'held')
    def test_input_overlap_and_reserved_internal_names_are_held(self):
        source=self.out/'input.json';source.write_bytes(b'input')
        self.assertNotEqual(self.invoke(source,input=source).returncode,0)
        for name in ('.physics-sim-headless-owner','wind_shot_manifest.json','wind_analysis_timeseries.jsonl','volume_frames/sidecar.json'):
            self.assertNotEqual(self.invoke(self.out/name).returncode,0)
        self.assertEqual(source.read_bytes(),b'input')
    def test_protected_linked_and_source_parents_are_held(self):
        repo=self.base/'repo';repo.mkdir();(repo/'.git').write_text('fixture');source=repo/'src';source.mkdir()
        linked=self.base/'link';linked.symlink_to(source)
        for path in (source/'summary.json',linked/'summary.json',Path('/usr/sidecar-test.json')):
            self.assertNotEqual(self.invoke(path).returncode,0)
        self.assertEqual(list(source.iterdir()),[])
    def test_preexisting_internal_file_requires_later_output_admission(self):
        path=self.out/'run_summary.json';path.write_bytes(b'old')
        self.assertEqual(self.invoke(path).returncode,0)
        self.assertNotEqual(self.invoke(path,'write').returncode,0);self.assertEqual(path.read_bytes(),b'old')
    def test_hardlinked_existing_leaf_is_held(self):
        original=self.base/'original';original.write_bytes(b'held');alias=self.out/'summary.json';os.link(original,alias)
        self.assertNotEqual(self.invoke(alias).returncode,0);self.assertEqual(original.read_bytes(),b'held')
class NativeCliPlan(unittest.TestCase):
    def setUp(self):
        selected=os.environ.get('PHYSICS_SIM_HEADLESS_BIN')
        if not selected:raise RuntimeError('PHYSICS_SIM_HEADLESS_BIN required for actual CLI proof')
        self.binary=Path(selected);self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup)
        self.base=Path(self.t.name);self.out=self.base/'output'
    def invoke(self,*args):
        return subprocess.run([str(self.binary),'--water-mode','--frames','1','--grid','4x4x4','--output-root',str(self.out),*map(str,args)],capture_output=True,text=True,timeout=15)
    def test_bad_plan_allocates_no_output_and_preserves_external_file(self):
        existing=self.base/'summary.json';existing.write_bytes(b'held')
        result=self.invoke('--summary',existing)
        self.assertNotEqual(result.returncode,0);self.assertIn('stage=prepare_sidecars',result.stderr)
        self.assertFalse(self.out.exists());self.assertEqual(existing.read_bytes(),b'held')
    def test_duplicate_plan_allocates_neither_root_nor_sidecar(self):
        same=self.base/'same.json';result=self.invoke('--summary',same,'--progress',same)
        self.assertNotEqual(result.returncode,0);self.assertFalse(self.out.exists());self.assertFalse(same.exists())
    def test_bad_sidecar_does_not_move_existing_completed_output(self):
        first=self.invoke();self.assertEqual(first.returncode,0,first.stdout+first.stderr)
        before=(self.out/'run_summary.json').read_bytes();existing=self.base/'foreign';existing.write_bytes(b'held')
        result=self.invoke('--overwrite','--progress',existing)
        self.assertNotEqual(result.returncode,0);self.assertEqual((self.out/'run_summary.json').read_bytes(),before)
        self.assertEqual(list(self.base.glob('output.retained-*')),[]);self.assertEqual(existing.read_bytes(),b'held')
    def test_fresh_external_sidecars_keep_operational_class(self):
        import json
        summary=self.base/'summary.json';progress=self.base/'progress.json'
        result=self.invoke('--summary',summary,'--progress',progress)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        for path in (summary,progress):self.assertEqual(json.loads(path.read_text())['artifact_class'],'operational_job')

if __name__=='__main__':unittest.main()
