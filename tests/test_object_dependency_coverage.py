"""Real object mapping admits extra headless and shape dependency closures."""
import os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ENV={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
class ObjectDependencies(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.repo=Path(self.t.name).resolve()
        for d in ('scripts','make','src/render','src/tools/cli'):(self.repo/d).mkdir(parents=True,exist_ok=True)
        for n in ('build_identity.py','build_outputs.py','clean_outputs.py','check_clean_root.py','atomic_output.py','build_owner.py','tool_probe.py'):shutil.copy2(ROOT/'scripts'/n,self.repo/'scripts'/n)
        for n in ('objects.mk','build-identity.mk'):shutil.copy2(ROOT/'make'/n,self.repo/'make'/n)
        self.variables='.DEFAULT_GOAL := all\nBUILD_DIR=build\nSRC_DIR=src\nCC=clang\nCLANG=clang\nCFLAGS=-O0\nSRCS=src/common.c\nPHYSICS_SIM_HEADLESS_TOOL_SRC=src/tools/cli/headless.c\nPHYSICS_SIM_JOB_RUNNER_TOOL_SRC=src/tools/cli/runner.c\nSHAPE_MASK_TOOL_SRC=src/tools/cli/mask.c\nSHAPE_ASSET_TOOL_SRC=src/tools/cli/asset.c\nSHAPE_SANITY_TOOL_SRC=src/tools/cli/sanity.c\n'
        lines=(ROOT/'make/rules-build.mk').read_text().splitlines();i=lines.index('$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c');self.rule='\n'.join(lines[i:i+3])+'\n'
    def make(self):return subprocess.run(['make'],cwd=self.repo,env=ENV,capture_output=True,text=True,timeout=60)
    def fixture(self,target,source):
        p=self.repo/source;p.write_text('#include "value.h"\nint value(void){return VALUE;}\n');header=p.parent/'value.h';header.write_text('#define VALUE 1\n')
        (self.repo/'Makefile').write_text(self.variables+'include make/objects.mk\nall: $('+target+')\n'+self.rule+'include make/build-identity.mk\n-include $(DEPS)\n')
        row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);obj=self.repo/('build/'+str(Path(source).relative_to('src').with_suffix('.o')));before=obj.read_bytes();info=header.stat();header.write_text('#define VALUE 2\n');os.utime(header,ns=(info.st_atime_ns,info.st_mtime_ns));row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertNotEqual(obj.read_bytes(),before)
        timestamp=obj.stat().st_mtime_ns;row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertEqual(obj.stat().st_mtime_ns,timestamp)
    def test_headless_renderer_stub_uses_real_mapping_and_content_rebuild(self):self.fixture('PHYSICS_SIM_HEADLESS_RENDERER_STUB_OBJ','src/render/renderer_sdl_headless_stub.c')
    def test_shape_tool_uses_real_mapping_and_content_rebuild(self):self.fixture('SHAPE_MASK_TOOL_OBJ','src/tools/cli/mask.c')
    def test_complete_declared_extra_object_set_has_deduplicated_dependencies(self):
        (self.repo/'Makefile').write_text(self.variables+'include make/objects.mk\n.PHONY: audit\n.DEFAULT_GOAL := audit\naudit:\n\t@printf "%s\\n" "$(DEPS)" "$(PHYSICS_SIM_HEADLESS_WORKER_OBJS) $(SHAPE_MASK_TOOL_OBJ) $(SHAPE_ASSET_TOOL_OBJ) $(SHAPE_SANITY_TOOL_OBJ) $(PHYSICS_SIM_HEADLESS_TOOL_OBJ) $(PHYSICS_SIM_JOB_RUNNER_TOOL_OBJ) $(BUILD_DIR)/tools/cli/physics_sim_session_worker.o"\n')
        row=self.make();self.assertEqual(row.returncode,0,row.stdout+row.stderr);deps,objects=row.stdout.splitlines()[-2:];items=deps.split();self.assertEqual(len(items),len(set(items)))
        for obj in objects.split():
            with self.subTest(obj=obj):self.assertIn(str(Path(obj).with_suffix('.d')),items)
if __name__=='__main__':unittest.main()
