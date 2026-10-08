"""Generated dependency admission occurs before executable Make inclusion."""
from unittest.mock import patch
import os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import build_outputs
ENV={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
class Dependencies(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.repo=Path(self.t.name).resolve()
        for d in ('scripts','make','src'):(self.repo/d).mkdir()
        for n in ('build_identity.py','build_outputs.py','clean_outputs.py','check_clean_root.py','atomic_output.py','build_owner.py','tool_probe.py'):shutil.copy2(ROOT/'scripts'/n,self.repo/'scripts'/n)
        shutil.copy2(ROOT/'make/build-identity.mk',self.repo/'make/build-identity.mk')
        lines=(ROOT/'make/rules-build.mk').read_text().splitlines();i=lines.index('$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c')
        (self.repo/'src/probe.c').write_text('#include "value.h"\nint value(void){return VALUE;}\n');(self.repo/'src/value.h').write_text('#define VALUE 1\n')
        (self.repo/'Makefile').write_text('BUILD_DIR=build\nSRC_DIR=src\nCC=clang\nCLANG=clang\nCFLAGS=-O0\nOBJS=build/probe.o\nDEPS=$(OBJS:.o=.d)\nall: $(OBJS)\n'+'\n'.join(lines[i:i+3])+'\ninclude make/build-identity.mk\n-include $(DEPS)\n')
        self.dep=self.repo/'build/probe.d';self.sentinel=self.repo/'executed'
    def make(self):return subprocess.run(['make'],cwd=self.repo,env=ENV,capture_output=True,text=True,timeout=60)
    def initial(self):
        row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr)
    def test_changed_dependency_bytes_hold_before_make_expression_executes(self):
        self.initial();self.dep.write_text(self.dep.read_text()+'\n$(shell touch executed)\n');before=(self.repo/'build/probe.o').read_bytes()
        row=self.make();self.assertFalse(self.sentinel.exists());self.assertNotEqual(row.returncode,0);self.assertEqual((self.repo/'build/probe.o').read_bytes(),before)
    def test_owned_dependency_expression_also_holds_before_execution(self):
        self.initial();self.dep.write_text(self.dep.read_text()+'\n$(shell touch executed)\n');build_outputs.record(self.repo,self.dep,'dependency')
        row=self.make();self.assertFalse(self.sentinel.exists());self.assertNotEqual(row.returncode,0)
    def test_missing_receipt_holds_without_recreating_or_overwriting(self):
        self.initial();receipt=build_outputs.receipt_path(self.repo,self.dep);receipt.unlink();before=self.dep.read_bytes()
        self.assertNotEqual(self.make().returncode,0);self.assertEqual(self.dep.read_bytes(),before);self.assertFalse(receipt.exists())
    def test_real_clang_dependencies_allow_noop_and_header_rebuild(self):
        self.initial();obj=self.repo/'build/probe.o';before=obj.stat().st_mtime_ns;content=obj.read_bytes()
        row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertEqual(obj.stat().st_mtime_ns,before)
        header=self.repo/'src/value.h';header.write_text('#define VALUE 2\n');os.utime(header,ns=(header.stat().st_atime_ns,obj.stat().st_mtime_ns+2000000000));row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertNotEqual(obj.read_bytes(),content)
    def test_owned_non_dependency_make_rules_hold_without_execution(self):
        self.initial();original=self.dep.read_text()
        for injected in ['\ninclude foreign.mk\n','\nprobe := replacement\n','\nother: src/probe.c\n','\n\ttouch executed\n','\nsrc/value.h: foreign\n','\n.PHONY: arbitrary\n']:
            with self.subTest(injected=injected):
                self.dep.write_text(original+injected);build_outputs.record(self.repo,self.dep,'dependency');row=self.make();self.assertNotEqual(row.returncode,0);self.assertFalse(self.sentinel.exists())
    def test_real_clang_escaped_space_header_is_admitted(self):
        (self.repo/'src/value space.h').write_text('#define VALUE 1\n');(self.repo/'src/probe.c').write_text('#include "value space.h"\nint value(void){return VALUE;}\n')
        self.initial();row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr)

    def test_prior_receipt_rewrite_holds_complete_dependency_pass(self):
        import build_identity as identity
        self.initial();other=self.repo/'build/other.d';other.write_text('build/other.o: src/probe.c\n');build_outputs.record(self.repo,other,'dependency')
        receipt=build_outputs.receipt_path(self.repo,self.dep);original=identity.verified;calls=0
        def changed(*args):
            nonlocal calls
            row=original(*args);calls+=1
            if calls==3:receipt.write_bytes(receipt.read_bytes())
            return row
        with patch.object(identity,'verified',side_effect=changed):
            with self.assertRaisesRegex(ValueError,'changed'):identity.admit_dependencies(self.repo,self.repo/'build',[self.dep,other])

    def test_changed_header_bytes_with_preserved_time_rebuild_and_matching_noop(self):
        self.initial();obj=self.repo/'build/probe.o';header=self.repo/'src/value.h';info=header.stat();old=obj.read_bytes();header.write_text('#define VALUE 2\n');os.utime(header,ns=(info.st_atime_ns,info.st_mtime_ns))
        row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertNotEqual(obj.read_bytes(),old)
        before=obj.stat().st_mtime_ns;row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertEqual(obj.stat().st_mtime_ns,before)

    def test_preserved_time_source_change_rebuilds_only_affected_object(self):
        makefile=self.repo/'Makefile';makefile.write_text(makefile.read_text().replace('OBJS=build/probe.o','OBJS=build/probe.o build/other.o'));(self.repo/'src/other.c').write_text('int other(void){return 3;}\n')
        self.initial();obj=self.repo/'build/probe.o';other=self.repo/'build/other.o';other_time=other.stat().st_mtime_ns;source=self.repo/'src/probe.c';info=source.stat();old=obj.read_bytes()
        source.write_text('#include "value.h"\nint value(void){return VALUE+1;}\n');os.utime(source,ns=(info.st_atime_ns,info.st_mtime_ns));row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertNotEqual(obj.read_bytes(),old);self.assertEqual(other.stat().st_mtime_ns,other_time)
    def test_legacy_owned_dependency_pair_rebuilds_once_then_noops(self):
        self.initial();obj=self.repo/'build/probe.o';before=obj.stat().st_mtime_ns;build_outputs.record(self.repo,self.dep,'dependency');build_outputs.record(self.repo,obj,'compiler')
        row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertNotEqual(obj.stat().st_mtime_ns,before)
        changed=obj.stat().st_mtime_ns;row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertEqual(obj.stat().st_mtime_ns,changed)
    def test_contradictory_input_snapshot_holds_without_output_mutation(self):
        import json
        self.initial();receipt=build_outputs.receipt_path(self.repo,self.dep);row=json.loads(receipt.read_text());row['input_snapshot']['count']=True;receipt.write_text(json.dumps(row));before=(self.repo/'build/probe.o').read_bytes()
        self.assertNotEqual(self.make().returncode,0);self.assertEqual((self.repo/'build/probe.o').read_bytes(),before)
    def test_input_size_preflight_precedes_any_input_hash(self):
        original=self.repo/'src/probe.c';other=self.repo/'src/large.h';other.write_bytes(b'');
        with other.open('r+b') as stream:stream.truncate(64*1024*1024+1)
        text='build/probe.o: src/probe.c src/large.h\n'
        with patch.object(build_outputs,'fingerprint',wraps=build_outputs.fingerprint) as hash_file:
            with self.assertRaisesRegex(ValueError,'input file bound'):build_outputs.dependency_input_snapshot(self.repo,self.dep,text)
            self.assertEqual(hash_file.call_count,0)

    def test_complete_input_pass_budget_holds_before_late_closure_hash(self):
        text='build/probe.o: src/probe.c\n';budget={'entries':0,'hash_bytes':0};size=(self.repo/'src/probe.c').stat().st_size
        with patch.object(build_outputs,'INPUT_PASS_MAX_HASH_BYTES',2*size):build_outputs.dependency_input_snapshot(self.repo,self.dep,text,budget)
        with patch.object(build_outputs,'INPUT_PASS_MAX_HASH_BYTES',2*size),patch.object(build_outputs,'fingerprint',wraps=build_outputs.fingerprint) as hash_file:
            with self.assertRaisesRegex(ValueError,'complete-pass budget'):build_outputs.dependency_input_snapshot(self.repo,self.dep,text,budget)
            self.assertEqual(hash_file.call_count,0)

if __name__=='__main__':unittest.main()
