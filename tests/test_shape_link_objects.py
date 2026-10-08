"""Shape tools use owned object dependencies and final linker provenance."""
import json,os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ENV={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
class ShapeLinkObjects(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.repo=Path(self.tmp.name).resolve()
        for d in ('scripts','make','src/tools/cli','src/import','src/app','vendor/timer/external'):(self.repo/d).mkdir(parents=True)
        for n in ('build_identity.py','build_outputs.py','clean_outputs.py','check_clean_root.py','atomic_output.py','build_owner.py','tool_probe.py'):shutil.copy2(ROOT/'scripts'/n,self.repo/'scripts'/n)
        for n in ('objects.mk','build-identity.mk'):shutil.copy2(ROOT/'make'/n,self.repo/'make'/n)
        self.cjson=self.repo/'vendor/timer/external/cJSON.c';self.header=self.cjson.with_name('value.h');self.cjson.write_text('#include "value.h"\nint helper(void){return VALUE;}\n');self.header.write_text('#define VALUE 1\n')
        (self.repo/'src/tools/cli/mask.c').write_text('int helper(void); int main(void){return helper();}\n')
        (self.repo/'src/tools/cli/asset.c').write_text('int helper(void); int main(void){return helper();}\n')
        def rule(name,file):
            lines=(ROOT/file).read_text().splitlines();i=next(i for i,line in enumerate(lines) if line.startswith(name+':'));selected=[lines[i]]
            for line in lines[i+1:]:
                if not line.startswith('\t'):break
                selected.append(line)
            return '\n'.join(selected)+'\n'
        (self.repo/'src/app/support.h').write_text('#define SUPPORT_VALUE 0\n')
        (self.repo/'src/app/physics_sim_persistence.c').write_text('#include \"support.h\"\nint support_value(void){return SUPPORT_VALUE;}\n')
        (self.repo/'src/app/physics_sim_headless_output.c').write_text('int output_dependency;\n')
        (self.repo/'src/app/input.h').write_text('#define INPUT_VALUE 0\n')
        (self.repo/'src/app/physics_sim_job_json.c').write_text('#include \"input.h\"\nint input_value(void){return INPUT_VALUE;}\n')
        (self.repo/'src/import/output.h').write_text('#define ASSET_OUTPUT_VALUE 0\n')
        (self.repo/'src/import/shape_asset_output.c').write_text('#include \"output.h\"\nint asset_output_value(void){return ASSET_OUTPUT_VALUE;}\n')
        (self.repo/'src/import/asset_input.h').write_text('#define ASSET_INPUT_VALUE 0\n')
        (self.repo/'src/import/shape_asset_input.c').write_text('#include "asset_input.h"\nint asset_input_value(void){return ASSET_INPUT_VALUE;}\n')
        self.variables='.DEFAULT_GOAL := all\nBUILD_DIR=build\nSRC_DIR=src\nUNAME_S='+('Darwin' if sys.platform=='darwin' else 'Linux')+'\nCC=clang\nCLANG=clang\nCFLAGS=-O0\nTIMER_HUD_DIR=vendor/timer\nTIMER_HUD_EXTERNAL_SRCS=vendor/timer/external/cJSON.c\nSHAPE_SHARED_SRCS=vendor/timer/external/cJSON.c\nSHAPE_MASK_TOOL_SRC=src/tools/cli/mask.c\nSHAPE_ASSET_TOOL_SRC=src/tools/cli/asset.c\nSHAPE_MASK_TOOL_BIN=build/bin/mask\nSHAPE_ASSET_TOOL_BIN=build/bin/asset\n'
        (self.repo/'Makefile').write_text(self.variables+'include make/objects.mk\nall: $(SHAPE_MASK_TOOL_BIN) $(SHAPE_ASSET_TOOL_BIN)\n'+rule('$(BUILD_DIR)/%.o','make/rules-build.mk')+rule('$(BUILD_DIR)/timer_hud_external/%.o','make/rules-build.mk')+rule('$(SHAPE_MASK_TOOL_BIN)','make/rules-tools.mk')+rule('$(SHAPE_ASSET_TOOL_BIN)','make/rules-tools.mk')+'include make/build-identity.mk\n-include $(DEPS)\n')
    def make(self):
        row=subprocess.run(['make'],cwd=self.repo,env=ENV,capture_output=True,text=True,timeout=60);self.assertEqual(row.returncode,0,row.stdout+row.stderr);return row
    def mutation(self,path,newtext):
        self.make();bins=[self.repo/'build/bin/mask',self.repo/'build/bin/asset'];self.assertEqual([subprocess.run([str(p)]).returncode for p in bins],[1,1])
        before=path.stat();path.write_text(newtext);os.utime(path,ns=(before.st_atime_ns,before.st_mtime_ns));self.make();self.assertEqual([subprocess.run([str(p)]).returncode for p in bins],[2,2])
        stamps=[p.stat().st_mtime_ns for p in bins];self.make();self.assertEqual([p.stat().st_mtime_ns for p in bins],stamps)
        self.assertTrue((self.repo/'build/timer_hud_external/cJSON.d').exists())
        if sys.platform=='darwin':
            for p in bins:self.assertTrue(p.with_name(p.name+'.link-inputs.json').exists())
    def test_external_header_preserved_time_rebuilds_both_tools(self):self.mutation(self.header,'#define VALUE 2\n')
    def test_external_source_preserved_time_rebuilds_both_tools(self):self.mutation(self.cjson,'#include "value.h"\nint helper(void){return 2;}\n')
    def test_publication_dependency_forces_both_shape_links(self):
        for name in ('mask','asset'):
            (self.repo/f'src/tools/cli/{name}.c').write_text('int helper(void); int support_value(void); int main(void){return helper()+support_value();}\n')
        self.make()
        mask=self.repo/'build/bin/mask';asset=self.repo/'build/bin/asset'
        self.assertEqual([subprocess.run([str(p)]).returncode for p in (mask,asset)],[1,1])
        header=self.repo/'src/app/support.h';before=header.stat()
        header.write_text('#define SUPPORT_VALUE 1\n');os.utime(header,ns=(before.st_atime_ns,before.st_mtime_ns))
        self.make()
        self.assertEqual([subprocess.run([str(p)]).returncode for p in (mask,asset)],[2,2])
        stamps=[p.stat().st_mtime_ns for p in (mask,asset)];self.make()
        self.assertEqual([p.stat().st_mtime_ns for p in (mask,asset)],stamps)
    def test_input_reader_dependency_forces_both_shape_links(self):
        for name in ('mask','asset'):
            (self.repo/f'src/tools/cli/{name}.c').write_text('int helper(void); int input_value(void); int main(void){return helper()+input_value();}\n')
        self.make();bins=[self.repo/'build/bin/mask',self.repo/'build/bin/asset']
        self.assertEqual([subprocess.run([str(p)]).returncode for p in bins],[1,1])
        header=self.repo/'src/app/input.h';before=header.stat();header.write_text('#define INPUT_VALUE 1\n');os.utime(header,ns=(before.st_atime_ns,before.st_mtime_ns))
        self.make();self.assertEqual([subprocess.run([str(p)]).returncode for p in bins],[2,2])
        stamps=[p.stat().st_mtime_ns for p in bins];self.make();self.assertEqual([p.stat().st_mtime_ns for p in bins],stamps)
    def test_asset_publication_helper_forces_both_shape_links(self):
        for name in ('mask','asset'):
            (self.repo/f'src/tools/cli/{name}.c').write_text('int helper(void); int asset_output_value(void); int main(void){return helper()+asset_output_value();}\n')
        self.make();bins=[self.repo/'build/bin/mask',self.repo/'build/bin/asset']
        self.assertEqual([subprocess.run([str(p)]).returncode for p in bins],[1,1])
        header=self.repo/'src/import/output.h';before=header.stat();header.write_text('#define ASSET_OUTPUT_VALUE 1\n');os.utime(header,ns=(before.st_atime_ns,before.st_mtime_ns))
        self.make();self.assertEqual([subprocess.run([str(p)]).returncode for p in bins],[2,2])
        stamps=[p.stat().st_mtime_ns for p in bins];self.make();self.assertEqual([p.stat().st_mtime_ns for p in bins],stamps)
    def test_asset_input_dependency_forces_both_shape_links(self):
        for name in ('mask','asset'):
            (self.repo/f'src/tools/cli/{name}.c').write_text('int helper(void); int asset_input_value(void); int main(void){return helper()+asset_input_value();}\n')
        self.make();bins=[self.repo/'build/bin/mask',self.repo/'build/bin/asset']
        self.assertEqual([subprocess.run([str(p)]).returncode for p in bins],[1,1])
        header=self.repo/'src/import/asset_input.h';before=header.stat();header.write_text('#define ASSET_INPUT_VALUE 1\n');os.utime(header,ns=(before.st_atime_ns,before.st_mtime_ns))
        self.make();self.assertEqual([subprocess.run([str(p)]).returncode for p in bins],[2,2])
        stamps=[p.stat().st_mtime_ns for p in bins];self.make();self.assertEqual([p.stat().st_mtime_ns for p in bins],stamps)
    def test_shared_link_declarations_contain_only_dependency_owned_objects(self):
        with (self.repo/'Makefile').open('a') as stream:stream.write('\naudit:\n\t@printf "%s\\n" "$(SHAPE_SHARED_OBJS)" "$(DEPS)"\n')
        row=subprocess.run(['make','audit'],cwd=self.repo,env=ENV,capture_output=True,text=True,timeout=60);self.assertEqual(row.returncode,0,row.stderr);objects,deps=row.stdout.splitlines()[-2:]
        self.assertEqual(objects.split(),['build/timer_hud_external/cJSON.o']);self.assertIn('build/timer_hud_external/cJSON.d',deps.split())
if __name__=='__main__':unittest.main()
