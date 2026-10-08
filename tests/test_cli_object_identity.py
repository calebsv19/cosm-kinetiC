"""Real Make direct CLI migrations track headers, sources and effective flags."""
import os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BEFORE=os.environ.get('PHYSICS_CLI_RULES_BEFORE')
ENV={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES','CPATH','C_INCLUDE_PATH','LIBRARY_PATH','PHYSICS_CLI_RULES_BEFORE')}
FAMILIES=[('VF2D_PACK_TOOL','CORE_PACK_TOOL_SRCS','vf2d_pack'),('VF2D_DATASET_TOOL','VF2D_DATASET_TOOL_SRCS','vf2d_dataset'),('PHYSICS_TRACE_TOOL','PHYSICS_TRACE_TOOL_SRCS','physics_trace'),('RUNTIME_SCENE_EMITTER_DIAG_TOOL','RUNTIME_SCENE_EMITTER_DIAG_TOOL_SRCS','emitter_diag')]
class CliObjects(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.base=Path(self.tmp.name).resolve()
    def fixture(self,family):
        tool,sources,namespace=family;repo=self.base/namespace
        for d in ('scripts','make','src','vendor'):(repo/d).mkdir(parents=True)
        for name in ('build_identity.py','build_outputs.py','clean_outputs.py','check_clean_root.py','atomic_output.py','build_owner.py','tool_probe.py'):shutil.copy2(ROOT/'scripts'/name,repo/'scripts'/name)
        origin=Path(BEFORE) if BEFORE else ROOT/'make'
        for name in ('objects.mk','build-identity.mk'):shutil.copy2(origin/name,repo/'make'/name)
        text=(origin/'rules-tools.mk').read_text();lines=text.splitlines();targets=['$('+tool+'_BIN)','$(BUILD_DIR)/tools/'+namespace+'/%.o'];rules=[]
        for target in targets:
            indices=[i for i,line in enumerate(lines) if line.startswith(target+':')]
            if not indices:continue
            i=indices[0];selected=[lines[i]]
            for line in lines[i+1:]:
                if not line.startswith('\t'):break
                selected.append(line)
            rules.append('\n'.join(selected)+'\n')
        header=repo/'vendor/value.h';header.write_text('#define VALUE 1\n');source=repo/'vendor/helper.c';source.write_text('#include "value.h"\n#ifndef OFFSET\n#define OFFSET 0\n#endif\nint helper(void){return VALUE+OFFSET;}\n');(repo/'src/probe.c').write_text('int helper(void); int main(void){return helper();}\n')
        variables='.DEFAULT_GOAL := all\nBUILD_DIR=build\nSRC_DIR=src\nCC=clang\nCLANG=clang\nCSTD=-std=c11\nCFLAGS=-std=c11 -O0\nDEBUG=-O0\nUNAME_S='+('Darwin' if sys.platform=='darwin' else 'Linux')+'\n'+sources+'=src/probe.c vendor/helper.c\n'+tool+'_BIN=build/bin/probe\n'+tool+'_CFLAGS=$(CSTD) $(DEBUG)\n'
        (repo/'Makefile').write_text(variables+'include make/objects.mk\nall: $('+tool+'_BIN)\n'+''.join(rules)+'include make/build-identity.mk\n-include $(DEPS)\n')
        return repo,header,source
    def make(self,repo,*args):
        row=subprocess.run(['make',*args],cwd=repo,env=ENV,capture_output=True,text=True,timeout=60);self.assertEqual(row.returncode,0,row.stdout+row.stderr);return row
    def proof(self,family):
        repo,header,source=self.fixture(family);self.make(repo);exe=repo/'build/bin/probe';self.assertEqual(subprocess.run([str(exe)]).returncode,1)
        for p,new,value in [(header,'#define VALUE 2\n',2),(source,'int helper(void){return 3;}\n',3)]:
            previous=p.stat();p.write_text(new);os.utime(p,ns=(previous.st_atime_ns,previous.st_mtime_ns));self.make(repo);self.assertEqual(subprocess.run([str(exe)]).returncode,value)
            stamp=exe.stat().st_mtime_ns;self.make(repo);self.assertEqual(exe.stat().st_mtime_ns,stamp)
        self.assertEqual(len(list((repo/'build').rglob('*.d'))),2)
        if sys.platform=='darwin':self.assertTrue(exe.with_name(exe.name+'.link-inputs.json').exists())
    def test_pack_preserved_header_and_source_changes_rebuild(self):self.proof(FAMILIES[0])
    def test_dataset_preserved_header_and_source_changes_rebuild(self):self.proof(FAMILIES[1])
    def test_trace_preserved_header_and_source_changes_rebuild(self):self.proof(FAMILIES[2])
    def test_emitter_preserved_header_and_source_changes_rebuild(self):self.proof(FAMILIES[3])
    def test_emitter_dot_component_sources_use_matching_rebuild_names(self):
        repo,header,source=self.fixture(FAMILIES[3]);(repo/'vendor/nested').mkdir();makefile=repo/'Makefile';makefile.write_text(makefile.read_text().replace('vendor/helper.c','vendor/nested/../helper.c'))
        self.make(repo);exe=repo/'build/bin/probe';before=header.stat();header.write_text('#define VALUE 2\n');os.utime(header,ns=(before.st_atime_ns,before.st_mtime_ns));self.make(repo);self.assertEqual(subprocess.run([str(exe)]).returncode,2)
        stamp=exe.stat().st_mtime_ns;self.make(repo);self.assertEqual(exe.stat().st_mtime_ns,stamp);self.assertTrue((repo/'build/tools/emitter_diag/vendor/helper.d').exists())
    def future_owned_binary(self,repo):
        import json,time
        sys.path.insert(0,str(ROOT/'scripts'));from build_outputs import receipt_path,record
        exe=repo/'build/bin/probe';owner=json.loads(receipt_path(repo,exe).read_text());future=time.time_ns()+3600*1000000000;os.utime(exe,ns=(future,future));record(repo,exe,'compiler',link_inputs_sha256=owner.get('link_inputs_sha256'));return exe
    def test_header_content_rebuild_forces_link_with_newer_owned_binary(self):
        repo,header,source=self.fixture(FAMILIES[3]);self.make(repo);exe=self.future_owned_binary(repo);before=header.stat();header.write_text('#define VALUE 2\n');os.utime(header,ns=(before.st_atime_ns,before.st_mtime_ns));self.make(repo);self.assertEqual(subprocess.run([str(exe)]).returncode,2)
    def test_configuration_change_forces_link_with_newer_owned_binary(self):
        repo,header,source=self.fixture(FAMILIES[3]);self.make(repo);exe=self.future_owned_binary(repo);self.make(repo,'RUNTIME_SCENE_EMITTER_DIAG_TOOL_CFLAGS=-std=c11 -O0 -DOFFSET=4');self.assertEqual(subprocess.run([str(exe)]).returncode,5)
    def test_effective_tool_flags_invalidate_each_profile(self):
        for family in FAMILIES:
            with self.subTest(tool=family[0]):
                repo,header,source=self.fixture(family);self.make(repo);exe=repo/'build/bin/probe';self.assertEqual(subprocess.run([str(exe)]).returncode,1)
                flag=family[0]+'_CFLAGS=-std=c11 -O0 -DOFFSET=4';self.make(repo,flag);self.assertEqual(subprocess.run([str(exe)]).returncode,5)
                stamp=exe.stat().st_mtime_ns;self.make(repo,flag);self.assertEqual(exe.stat().st_mtime_ns,stamp)
    def test_all_tool_objects_are_declared_and_have_dependencies(self):
        for family in FAMILIES:
            with self.subTest(tool=family[0]):
                repo,_,_=self.fixture(family)
                with (repo/'Makefile').open('a') as stream:stream.write('\naudit:\n\t@printf "%s\\n" "$('+family[0]+'_OBJS)" "$(DEPS)"\n')
                row=self.make(repo,'audit');objects,deps=row.stdout.splitlines()[-2:];self.assertEqual(len(objects.split()),2)
                for name in objects.split():self.assertIn(str(Path(name).with_suffix('.d')),deps.split())
if __name__=='__main__':unittest.main()
