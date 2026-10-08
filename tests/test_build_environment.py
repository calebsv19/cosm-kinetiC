"""Real Make rebuilds when implicit compiler include/link environment changes."""
import json,os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
ENV={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
class BuildEnvironment(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.repo=Path(self.t.name).resolve()
        for directory in ('scripts','make','src','one','two'):(self.repo/directory).mkdir()
        for name in ('tool_probe.py','build_identity.py','build_outputs.py','clean_outputs.py','check_clean_root.py','atomic_output.py','build_owner.py'):shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        shutil.copy2(ROOT/'make/build-identity.mk',self.repo/'make/build-identity.mk')
        lines=(ROOT/'make/rules-build.mk').read_text().splitlines();i=lines.index('$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c');rule='\n'.join(lines[i:i+3])+'\n'
        (self.repo/'Makefile').write_text('BUILD_DIR=build\nSRC_DIR=src\nCC=clang\nCLANG=clang\nCFLAGS=-O0\nOBJS=build/probe.o\nall: $(OBJS)\n'+rule+'include make/build-identity.mk\n')
    def run_command(self,argv,env=None):
        row=subprocess.run(argv,cwd=self.repo,env=env or ENV,capture_output=True,text=True,timeout=60);self.assertEqual(row.returncode,0,row.stdout+row.stderr);return row
    def selection(self):return json.loads((self.repo/'build/.configuration/active.json').read_text())
    def test_real_include_environment_change_rebuilds_and_matching_noop(self):
        (self.repo/'src/probe.c').write_text('#include <probe_value.h>\nint value(void){return VALUE;}\n')
        for name,value in [('one',1),('two',2)]:(self.repo/name/'probe_value.h').write_text('#define VALUE '+str(value)+'\n')
        for variable in ('CPATH','C_INCLUDE_PATH'):
            with self.subTest(variable=variable):
                a=dict(ENV,**{variable:str(self.repo/'one')});b=dict(ENV,**{variable:str(self.repo/'two')})
                self.run_command(['make'],a);obj=self.repo/'build/probe.o';first=obj.read_bytes();before=obj.stat().st_mtime_ns;selected=self.selection()
                self.run_command(['make'],a);self.assertEqual(obj.stat().st_mtime_ns,before);self.assertEqual(self.selection(),selected)
                self.run_command(['make'],b);self.assertNotEqual(obj.read_bytes(),first);self.assertNotEqual(self.selection(),selected)
                changed=obj.stat().st_mtime_ns;self.run_command(['make'],b);self.assertEqual(obj.stat().st_mtime_ns,changed)
    def test_real_library_search_environment_change_relinks(self):
        for name,value in [('one',1),('two',2)]:
            src=self.repo/name/'value.c';src.write_text('int value(void){return '+str(value)+';}\n')
            self.run_command(['clang','-c',str(src),'-o',str(src.with_suffix('.o'))]);self.run_command(['ar','rcs',str(self.repo/name/'libfixture.a'),str(src.with_suffix('.o'))])
        (self.repo/'src/probe.c').write_text('int value(void); int main(void){return value();}\n')
        p=self.repo/'Makefile';s=p.read_text().replace('include make/build-identity.mk\n','CLANG_TARGET=build/bin/probe\nLIBS=-lfixture\nall: $(CLANG_TARGET)\n$(CLANG_TARGET): $(OBJS)\n\t@mkdir -p $(dir $@)\n\tpython3 -B scripts/atomic_output.py -- $(CC) $(OBJS) $(LIBS) -o $@\ninclude make/build-identity.mk\n');p.write_text(s)
        a=dict(ENV,LIBRARY_PATH=str(self.repo/'one'));b=dict(ENV,LIBRARY_PATH=str(self.repo/'two'))
        self.run_command(['make'],a);exe=self.repo/'build/bin/probe';self.assertEqual(subprocess.run([str(exe)]).returncode,1);before=exe.stat().st_mtime_ns
        self.run_command(['make'],a);self.assertEqual(exe.stat().st_mtime_ns,before)
        self.run_command(['make'],b);self.assertEqual(subprocess.run([str(exe)]).returncode,2);changed=exe.stat().st_mtime_ns
        self.run_command(['make'],b);self.assertEqual(exe.stat().st_mtime_ns,changed)
    def test_context_preserves_absence_empty_and_exact_path_order(self):
        sys.path.insert(0,str(ROOT/'scripts'));import build_identity as identity
        with patch.dict(os.environ,{},clear=True):self.assertIsNone(identity.compiler_environment()['CPATH'])
        with patch.dict(os.environ,{'CPATH':'','C_INCLUDE_PATH':' second:first: '},clear=True):
            row=identity.compiler_environment();self.assertEqual(row['CPATH'],'');self.assertEqual(row['C_INCLUDE_PATH'],' second:first: ')
    def test_context_byte_limits_are_inclusive_and_prepublication(self):
        sys.path.insert(0,str(ROOT/'scripts'));import build_identity as identity
        with patch.dict(os.environ,{'CPATH':'é'*32768},clear=True):self.assertEqual(len(identity.compiler_environment()['CPATH'].encode()),65536)
        with patch.dict(os.environ,{'CPATH':'é'*32768+'x'},clear=True):
            with self.assertRaisesRegex(ValueError,'value byte bound'):identity.compiler_environment()
        values={key:'x'*65536 for key in identity.COMPILER_ENVIRONMENT_KEYS[:16]}
        with patch.dict(os.environ,values,clear=True):identity.compiler_environment()
        values[identity.COMPILER_ENVIRONMENT_KEYS[16]]='x'
        with patch.dict(os.environ,values,clear=True):
            with self.assertRaisesRegex(ValueError,'aggregate byte bound'):identity.compiler_environment()

if __name__=='__main__':unittest.main()
