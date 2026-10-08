"""Actual compilation input mutations hold publication and retain candidates."""
import json,os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from build_outputs import receipt_path
class CompilerBoundary(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.repo=Path(self.t.name).resolve();(self.repo/'scripts').mkdir();(self.repo/'build').mkdir()
        for name in ('atomic_output.py','build_owner.py','build_outputs.py','clean_outputs.py','check_clean_root.py'):shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        (self.repo/'probe.c').write_text('#include "value.h"\nint value(void){return VALUE;}\n');self.header=self.repo/'value.h';self.header.write_text('#define VALUE 1\n')
        self.obj=self.repo/'build/probe.o';self.dep=self.obj.with_suffix('.d')
    def compile(self,compiler='clang',*prefix):
        return subprocess.run([sys.executable,'-B','scripts/atomic_output.py','--',compiler,*prefix,'-MMD','-MP','-c','probe.c','-o','build/probe.o'],cwd=self.repo,capture_output=True,text=True,timeout=60)
    def wrapper(self,mode):
        path=self.repo/'compiler.py';path.write_text('import pathlib,subprocess,sys\nargs=sys.argv[1:]\ncode=subprocess.call(["clang",*args])\nif code==0 and "-c" in args:\n p=pathlib.Path("value.h")\n p.write_text('+('"#define VALUE 2\\n"' if mode=='changed' else 'p.read_text()')+')\nsys.exit(code)\n');return path
    def check_mutation(self,mode):
        first=self.compile();self.assertEqual(first.returncode,0,first.stderr)
        paths=[self.obj,self.dep,receipt_path(self.repo,self.obj),receipt_path(self.repo,self.dep)];before={p:p.read_bytes() for p in paths}
        wrapper=self.wrapper(mode);row=self.compile(sys.executable,str(wrapper));self.assertNotEqual(row.returncode,0,row.stderr)
        self.assertEqual({p:p.read_bytes() for p in paths},before)
        held=list((self.repo/'build').glob('.probe.o.staging-*/input-hold.json'));self.assertEqual(len(held),1);receipt=json.loads(held[0].read_text());self.assertEqual(receipt['artifact_class'],'retained_compiler_output');self.assertTrue((held[0].parent/'probe.o').exists())
    def test_changed_header_after_actual_clang_holds_predecessor_and_candidate(self):self.check_mutation('changed')
    def test_same_byte_header_rewrite_after_actual_clang_holds(self):self.check_mutation('same')
    def test_fresh_mutation_does_not_publish_output_or_receipts(self):
        wrapper=self.wrapper('changed');row=self.compile(sys.executable,str(wrapper));self.assertNotEqual(row.returncode,0);self.assertFalse(self.obj.exists());self.assertFalse(self.dep.exists());self.assertFalse(receipt_path(self.repo,self.obj).exists())
    def test_normal_compile_records_boundary_scope_and_cleans_temporary_stage(self):
        row=self.compile();self.assertEqual(row.returncode,0,row.stderr);owner=json.loads(receipt_path(self.repo,self.dep).read_text());self.assertEqual(owner['input_snapshot']['scope'],'compiler_boundary_observation');self.assertEqual(list((self.repo/'build').glob('.probe.o.staging-*')),[])
    def test_disappeared_header_after_compile_retains_candidate(self):
        first=self.compile();self.assertEqual(first.returncode,0,first.stderr);before=self.obj.read_bytes()
        wrapper=self.repo/'remove.py';wrapper.write_text('import pathlib,subprocess,sys\nargs=sys.argv[1:]\ncode=subprocess.call(["clang",*args])\nif code==0 and "-c" in args:pathlib.Path("value.h").unlink()\nsys.exit(code)\n')
        row=self.compile(sys.executable,str(wrapper));self.assertNotEqual(row.returncode,0);self.assertEqual(self.obj.read_bytes(),before);self.assertEqual(len(list((self.repo/'build').glob('.probe.o.staging-*/input-hold.json'))),1)
    def test_preparation_failure_does_not_start_actual_compile(self):
        first=self.compile();self.assertEqual(first.returncode,0,first.stderr);before=self.obj.read_bytes()
        wrapper=self.repo/'prepare_fail.py';wrapper.write_text('import pathlib,sys\nif "-MM" in sys.argv:sys.exit(5)\npathlib.Path("compiled").touch()\nsys.exit(0)\n')
        row=self.compile(sys.executable,str(wrapper));self.assertEqual(row.returncode,5);self.assertFalse((self.repo/'compiled').exists());self.assertEqual(self.obj.read_bytes(),before)
    def test_md_discovery_matches_system_header_closure(self):
        (self.repo/'probe.c').write_text('#include <stdbool.h>\n#include "value.h"\nint value(void){return VALUE;}\n')
        row=subprocess.run([sys.executable,'-B','scripts/atomic_output.py','--','clang','-MD','-MP','-c','probe.c','-o','build/probe.o'],cwd=self.repo,capture_output=True,text=True,timeout=60);self.assertEqual(row.returncode,0,row.stderr)
        owner=json.loads(receipt_path(self.repo,self.dep).read_text());self.assertGreater(owner['input_snapshot']['count'],2);self.assertEqual(owner['input_snapshot']['scope'],'compiler_boundary_observation')

if __name__=='__main__':unittest.main()
