"""Two real Make builds own independent file targets; aliases do not relink."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
ENV={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}

class Isolation(unittest.TestCase):
    def test_concurrent_roots_and_incremental_selection(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary)
            for name in ('scripts','make','src'): (repo/name).mkdir()
            for name in ('tool_probe.py','atomic_output.py', 'build_owner.py','build_identity.py','build_outputs.py','clean_outputs.py','check_clean_root.py'):
                shutil.copy2(ROOT/'scripts'/name,repo/'scripts'/name)
            for name in ('config.mk','sources-tools.mk','build-identity.mk'):
                shutil.copy2(ROOT/'make'/name,repo/'make'/name)
            for name in ('VERSION','WORKER_VERSION'): (repo/name).write_text('test\n')
            (repo/'src/probe.c').write_text('int main(void){return VALUE;}\n')
            (repo/'physics_sim_headless').write_bytes(b'legacy binary preserved')
            lines=(ROOT/'make/rules-build.mk').read_text().splitlines();start=lines.index('$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c');compile_rule='\n'.join(lines[start:start+3])+'\n'
            lines=(ROOT/'make/rules-tools.mk').read_text().splitlines();start=lines.index('$(PHYSICS_SIM_HEADLESS_TOOL_BIN): $(PHYSICS_SIM_HEADLESS_TOOL_OBJ) $(PHYSICS_SIM_HEADLESS_WORKER_OBJS)');link_rule='\n'.join(lines[start:start+2])+'\n'
            (repo/'Makefile').write_text('include make/config.mk\ninclude make/sources-tools.mk\nCFLAGS=-DVALUE=$(VALUE)\nPHYSICS_SIM_HEADLESS_TOOL_OBJ=$(BUILD_DIR)/probe.o\nall: physics_sim_headless\n.PHONY: physics_sim_headless\nphysics_sim_headless: $(PHYSICS_SIM_HEADLESS_TOOL_BIN)\n'+compile_rule+link_rule+'include make/build-identity.mk\n')
            processes=[subprocess.Popen(['make','BUILD_DIR=build/'+name,'VALUE='+value],cwd=repo,env=ENV,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for name,value in [('a','7'),('b','9')]]
            try:
                results=[(process,process.communicate(timeout=30)) for process in processes]
                for process,(out,err) in results:
                    self.assertEqual(process.returncode,0,out+err)
            finally:
                for process in processes:
                    if process.poll() is None:process.kill()
                    process.communicate(timeout=30)
            snapshots={}
            for name,value in [('a',7),('b',9)]:
                binary=repo/'build'/name/'bin/physics_sim_headless'
                self.assertEqual(subprocess.run([str(binary)]).returncode,value)
                snapshots[name]=(binary.read_bytes(),binary.stat().st_mtime_ns)
            for name,value in [('a','7'),('b','9')]:
                result=subprocess.run(['make','BUILD_DIR=build/'+name,'VALUE='+value],cwd=repo,env=ENV,capture_output=True,text=True,timeout=30)
                self.assertEqual(result.returncode,0,result.stderr)
                for other in ('a','b'):
                    p=repo/'build'/other/'bin/physics_sim_headless';self.assertEqual((p.read_bytes(),p.stat().st_mtime_ns),snapshots[other])
            self.assertEqual((repo/'physics_sim_headless').read_bytes(),b'legacy binary preserved')

if __name__=='__main__':unittest.main()
