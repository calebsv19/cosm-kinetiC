"""Native final link reuse and publication admission, using current Make helpers."""
import importlib.util,json,os,subprocess,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from build_outputs import receipt_path,linker_dependency_records,link_input_snapshot
spec=importlib.util.spec_from_file_location('link_fixture',ROOT/'tests/test_build_environment.py');fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
@unittest.skipUnless(sys.platform=='darwin','Darwin linker manifest contract')
class LinkIdentity(unittest.TestCase):
    setUp=fixture.BuildEnvironment.setUp
    run_command=fixture.BuildEnvironment.run_command
    def library(self,directory,value):
        src=self.repo/directory/'value.c';src.write_text('int value(void){return '+str(value)+';}\n')
        self.run_command(['clang','-c',str(src),'-o',str(src.with_suffix('.o'))]);self.run_command(['ar','rcs',str(src.parent/'libfixture.a'),str(src.with_suffix('.o'))])
    def setup_link(self):
        self.library('two',1);(self.repo/'src/probe.c').write_text('int value(void); int main(void){return value();}\n')
        p=self.repo/'Makefile';s=p.read_text().replace('include make/build-identity.mk\n','UNAME_S=Darwin\nCLANG_TARGET=build/bin/probe\nLIBS=-Lone -Ltwo -lfixture\nall: $(CLANG_TARGET)\n$(CLANG_TARGET): $(OBJS)\n\t@mkdir -p $(dir $@)\n\tpython3 -B scripts/atomic_output.py $(LINK_PROVENANCE_FLAG) -- $(CC) $(OBJS) $(LIBS) -o $@\ninclude make/build-identity.mk\n');p.write_text(s)
        self.run_command(['make']);self.exe=self.repo/'build/bin/probe';self.manifest=self.exe.with_name('probe.link-inputs.json')
    def result(self):return subprocess.run([str(self.exe)]).returncode
    def test_same_archive_preserved_time_relinks_then_matching_noop(self):
        self.setup_link();self.assertEqual(self.result(),1);lib=self.repo/'two/libfixture.a';before=lib.stat();self.library('two',2);os.utime(lib,ns=(before.st_atime_ns,before.st_mtime_ns))
        self.run_command(['make']);self.assertEqual(self.result(),2);stamp=self.exe.stat().st_mtime_ns;self.run_command(['make']);self.assertEqual(self.exe.stat().st_mtime_ns,stamp)
    def test_earlier_absent_archive_appearance_relinks(self):
        self.setup_link();self.library('one',3);self.run_command(['make']);self.assertEqual(self.result(),3)
        stamp=self.exe.stat().st_mtime_ns;self.run_command(['make']);self.assertEqual(self.exe.stat().st_mtime_ns,stamp)
    def test_bound_manifest_missing_holds_existing_binary(self):
        self.setup_link();before=self.exe.read_bytes();self.manifest.unlink()
        row=subprocess.run(['make'],cwd=self.repo,env=fixture.ENV,capture_output=True,text=True,timeout=60);self.assertNotEqual(row.returncode,0);self.assertIn('Missing bound linker manifest',row.stderr);self.assertEqual(self.exe.read_bytes(),before)
    def test_legacy_output_refreshes_once(self):
        self.setup_link();self.manifest.unlink();owner=receipt_path(self.repo,self.exe);row=json.loads(owner.read_text());del row['link_inputs_sha256'];owner.write_text(json.dumps(row))
        self.run_command(['make']);self.assertTrue(self.manifest.exists());stamp=self.exe.stat().st_mtime_ns;self.run_command(['make']);self.assertEqual(self.exe.stat().st_mtime_ns,stamp)
    def test_library_mutation_after_actual_link_holds_predecessor_and_candidate(self):
        self.setup_link();paths=[self.exe,self.manifest,receipt_path(self.repo,self.exe),receipt_path(self.repo,self.manifest)];before={p:p.read_bytes() for p in paths}
        wrapper=self.repo/'linker.py';wrapper.write_text('import pathlib,subprocess,sys\nargs=sys.argv[1:]\ncode=subprocess.call(["clang",*args])\nif code==0 and not any(x.endswith("discovery.bin") for x in args):\n p=pathlib.Path("two/libfixture.a");p.write_bytes(p.read_bytes())\nsys.exit(code)\n')
        row=subprocess.run([sys.executable,'-B','scripts/atomic_output.py','--link-provenance','--',sys.executable,str(wrapper),'build/probe.o','-Lone','-Ltwo','-lfixture','-o','build/bin/probe'],cwd=self.repo,capture_output=True,text=True,timeout=60)
        self.assertNotEqual(row.returncode,0,row.stderr);self.assertEqual({p:p.read_bytes() for p in paths},before)
        held=list((self.repo/'build/bin').glob('.probe.staging-*/link-input-hold.json'));self.assertEqual(len(held),1);self.assertTrue((held[0].parent/'probe').exists())
    def test_installed_trace_normalizes_relative_misses_and_deduplicates(self):
        self.setup_link();row=json.loads(self.manifest.read_text());selection=row['selection'];self.assertIn(str(self.repo/'one/libfixture.a'),selection['absent']);self.assertIn(str(self.repo/'two/libfixture.a'),selection['selected']);self.assertEqual(selection['selected'],sorted(set(selection['selected'])))
    def test_parser_refuses_unknown_truncated_duplicate_and_control_records(self):
        trace=self.repo/'trace.dep'
        for data in [b'\x00ld\0\x12evil\0',b'\x00ld\0\x10unterminated',b'\x00ld\0\x00ld\0',b'\x00ld\0\x10bad\npath\0',b'\x10path\0']:
            with self.subTest(data=data):
                trace.write_bytes(data)
                with self.assertRaises(ValueError):linker_dependency_records(trace,self.repo)
    def test_manifest_binding_contradiction_holds(self):
        self.setup_link();owner=receipt_path(self.repo,self.exe);row=json.loads(owner.read_text());row['link_inputs_sha256']='0'*64;owner.write_text(json.dumps(row))
        before=self.exe.read_bytes();result=subprocess.run(['make'],cwd=self.repo,env=fixture.ENV,capture_output=True,text=True,timeout=60)
        self.assertNotEqual(result.returncode,0);self.assertIn('binding mismatch',result.stderr);self.assertEqual(self.exe.read_bytes(),before)
    def test_validly_owned_manifest_count_contradiction_holds(self):
        from build_outputs import record
        self.setup_link();row=json.loads(self.manifest.read_text());row['snapshot']['selected_count']+=1;self.manifest.write_text(json.dumps(row));owned=record(self.repo,self.manifest,'linker-inputs');record(self.repo,self.exe,'compiler',link_inputs_sha256=owned['sha256'])
        result=subprocess.run(['make'],cwd=self.repo,env=fixture.ENV,capture_output=True,text=True,timeout=60)
        self.assertNotEqual(result.returncode,0);self.assertIn('count mismatch',result.stderr)
    def test_absent_search_candidate_appearance_during_link_holds(self):
        self.setup_link();before=self.exe.read_bytes();wrapper=self.repo/'appear.py'
        wrapper.write_text('import pathlib,subprocess,sys\nargs=sys.argv[1:]\ncode=subprocess.call(["clang",*args])\nif code==0 and not any(x.endswith("discovery.bin") for x in args):pathlib.Path("one/libfixture.a").write_bytes(pathlib.Path("two/libfixture.a").read_bytes())\nsys.exit(code)\n')
        row=subprocess.run([sys.executable,'-B','scripts/atomic_output.py','--link-provenance','--',sys.executable,str(wrapper),'build/probe.o','-Lone','-Ltwo','-lfixture','-o','build/bin/probe'],cwd=self.repo,capture_output=True,text=True,timeout=60)
        self.assertNotEqual(row.returncode,0,row.stderr);self.assertEqual(self.exe.read_bytes(),before);self.assertEqual(len(list((self.repo/'build/bin').glob('.probe.staging-*/link-input-hold.json'))),1)
    def test_fresh_mutation_publishes_neither_output_nor_manifest(self):
        self.setup_link();fresh=self.repo/'build/bin/fresh';wrapper=self.repo/'fresh.py'
        wrapper.write_text('import pathlib,subprocess,sys\nargs=sys.argv[1:]\ncode=subprocess.call(["clang",*args])\nif code==0 and not any(x.endswith("discovery.bin") for x in args):\n p=pathlib.Path("two/libfixture.a");p.write_bytes(p.read_bytes())\nsys.exit(code)\n')
        row=subprocess.run([sys.executable,'-B','scripts/atomic_output.py','--link-provenance','--',sys.executable,str(wrapper),'build/probe.o','-Lone','-Ltwo','-lfixture','-o',str(fresh)],cwd=self.repo,capture_output=True,text=True,timeout=60)
        self.assertNotEqual(row.returncode,0,row.stderr);self.assertFalse(fresh.exists());self.assertFalse(fresh.with_name('fresh.link-inputs.json').exists());self.assertFalse(receipt_path(self.repo,fresh).exists());self.assertEqual(len(list((self.repo/'build/bin').glob('.fresh.staging-*/link-input-hold.json'))),1)
    def test_trace_output_binding_and_secondary_output_refusal(self):
        trace=self.repo/'trace.dep';trace.write_bytes(b'\x00ld\0\x10/input.o\0\x40/wrong.bin\0')
        with self.assertRaisesRegex(ValueError,'staged publication'):linker_dependency_records(trace,self.repo,self.repo/'expected.bin')
        row=subprocess.run([sys.executable,'-B','scripts/atomic_output.py','--link-provenance','--','clang','object.o','-Wl,-map,external.map','-o','build/probe'],cwd=self.repo,capture_output=True,text=True,timeout=60)
        self.assertNotEqual(row.returncode,0);self.assertIn('secondary link outputs',row.stderr);self.assertFalse((self.repo/'external.map').exists())
    def test_snapshot_bounds_precede_hashing(self):
        from unittest.mock import patch
        large=self.repo/'large.a'
        with large.open('wb') as stream:stream.truncate(64*1024*1024+1)
        selection={'schema':'physics_sim_link_selection_v1','linker_version':'ld','selected':[str(large)],'absent':[]}
        with patch('build_outputs.fingerprint',side_effect=AssertionError('hash before bounds')):
            with self.assertRaisesRegex(ValueError,'file bound'):link_input_snapshot(selection)
if __name__=='__main__':unittest.main()
