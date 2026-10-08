"""Execute migrated Make recipe bodies and verify ownership and failure holds."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from build_outputs import inventory

TARGETS=('test-session-observation','test-solver-qualification','test-cfd-channel',
    'test-cfd-mac2d','test-cfd-surface-force','test-cfd-mac2d-obstacle',
    'test-cfd-mac2d-obstacle-step','test-cfd-mac2d-boundary-pressure',
    'test-cfd-mac2d-force-accuracy','test-cfd-open2d','test-cfd-open2d-exit',
    'test-cfd-mac2d-corner','test-cfd-open2d-obstacle','test-cfd-open2d-budget',
    'test-cfd-open2d-exit-centered','test-cfd-momentum-flux','test-cfd-open2d-exit-donor',
    'test-cfd-open2d-reference','test-cfd-open2d-force-check','test-cfd-masked-energy',
    'test-cfd-masked-energy-transient','test-cfd-pressure-guess','test-cfd-pressure-scaling',
    'test-cfd-pressure-mg',
    'test-cfd-refined-mesh',
    'test-cfd-refined-diffusion',
    'test-cfd-refined-projection',
    'test-cfd-refined-transient',
    'test-cfd-refined-transport',
    'test-cfd-refined-channel',
    'probe-cfd-refined-flow-transient',
    'test-cfd-refined-split-channel',
    'test-cfd-refined-channel-units',
    'test-cfd-refined-energy',
    'test-cfd-refined-obstacle-evolution',
    'probe-cfd-refined-obstacle',
    'test-cfd-obstacle3d-pressure-trace-api',
    'test-cfd-obstacle3d-pressure-trace-api-sanitize',
    'test-cfd-obstacle3d-pressure-trace-anisotropic',
    'test-cfd-obstacle3d-pressure-trace-anisotropic-sanitize',
    'test-cfd-obstacle3d-material-scaling',
    'test-cfd-obstacle3d-material-scaling-sanitize',
    'test-passive-atmosphere-native',
    'test-passive-atmosphere-sanitize',
    'test-evolving-atmosphere-native',
    'test-evolving-atmosphere-sanitize',
    'test-open-atmosphere-native',
    'test-open-atmosphere-sanitize')


class ContractWriters(unittest.TestCase):
    def test_actual_recipe_bodies_register_outputs_and_hold_unknown_or_failed_rebuild(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();(repo/'scripts').mkdir()
            for name in ('atomic_output.py','build_owner.py','build_outputs.py','clean_outputs.py','check_clean_root.py'):
                shutil.copy2(ROOT/'scripts'/name,repo/'scripts'/name)
            bodies=[];active=False
            for line in (ROOT/'make/rules-tools.mk').read_text().splitlines():
                if line and not line.startswith(('\t',' ','#')):
                    active=line.split(':')[0] in TARGETS
                    if active:bodies.append(line)
                elif active:bodies.append(line)
            compiler=repo/'compiler.py'
            compiler.write_text("import pathlib,sys\npathlib.Path('compiler-invocations').open('a').write('called\\n')\nout=pathlib.Path(sys.argv[sys.argv.index('-o')+1])\nout.write_text('#!/bin/sh\\nexit 0\\n');out.chmod(0o755)\nraise SystemExit(1 if pathlib.Path('fail-compiler').exists() else 0)\n")
            (repo/'Makefile').write_text('BUILD_DIR=build/profile\nCC='+sys.executable+' -B compiler.py\n'+ '\n'.join(bodies)+'\n')
            env={key:value for key,value in os.environ.items() if key not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES','PHYSICS_SIM_BUILD_OWNER_ROOT','PHYSICS_SIM_BUILD_OWNER_FDS')}
            def make(*targets):
                return subprocess.run(['make',*targets],cwd=repo,env=env,text=True,capture_output=True,timeout=40)
            result=make(*TARGETS)
            self.assertEqual(result.returncode,0,result.stderr)
            owned=inventory(repo,repo/'build/profile')
            self.assertEqual(len(owned),50)
            for target,binary in (('test-cfd-channel','cfd_channel_test'),
                                  ('test-cfd-refined-mesh','cfd_refined_mesh_test'),
                                  ('test-cfd-obstacle3d-pressure-trace-api','c3d-pressure-trace-api/contract'),
                                  ('test-passive-atmosphere-sanitize','passive-atmosphere/contract-sanitize'),
                                  ('test-open-atmosphere-native','open-atmosphere/contract-test')):
                output=repo/'build/profile'/binary;before=output.read_bytes()
                (repo/'fail-compiler').touch()
                result=make(target)
                self.assertNotEqual(result.returncode,0)
                self.assertEqual(output.read_bytes(),before)
                (repo/'fail-compiler').unlink()
                output.write_bytes(b'unknown replacement')
                calls=(repo/'compiler-invocations').read_bytes()
                result=make(target)
                self.assertNotEqual(result.returncode,0)
                self.assertEqual((repo/'compiler-invocations').read_bytes(),calls)
                self.assertEqual(output.read_bytes(),b'unknown replacement')


if __name__=='__main__':unittest.main()
