"""Compiler configuration drift changes the actual owning object rule's output."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIXTURE_ENV = {k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES')}


class BuildIdentity(unittest.TestCase):
    def test_changed_flags_rebuild_unchanged_configuration_does_not(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            (repo/'src').mkdir(); (repo/'scripts').mkdir(); (repo/'make').mkdir()
            (repo/'src/probe.c').write_text('int value(void){return VALUE;}\n')
            for name in ('tool_probe.py','build_identity.py', 'build_outputs.py', 'clean_outputs.py', 'check_clean_root.py', 'atomic_output.py', 'build_owner.py'):
                shutil.copy2(ROOT/'scripts'/name, repo/'scripts'/name)
            shutil.copy2(ROOT/'make/build-identity.mk', repo/'make/build-identity.mk')
            lines=(ROOT/'make/rules-build.mk').read_text().splitlines()
            start=lines.index('$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c')
            rule='\n'.join(lines[start:start+3])+'\n'
            (repo/'Makefile').write_text('BUILD_DIR=build\nSRC_DIR=src\nCC=clang\nCLANG=clang\nCFLAGS ?= -DVALUE=1\nOBJS=build/probe.o\nall: $(OBJS)\n'+rule+'include make/build-identity.mk\n')
            def make(*arguments):
                result=subprocess.run(['make', *arguments],cwd=repo,env=FIXTURE_ENV,capture_output=True,text=True,timeout=60)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            make(); obj=repo/'build/probe.o';stamp=next((repo/'build/.configuration').glob('*-*.json'))
            first=obj.read_bytes(); before=(obj.stat().st_mtime_ns,stamp.stat().st_mtime_ns)
            make();self.assertEqual((obj.stat().st_mtime_ns,stamp.stat().st_mtime_ns),before)
            make('CFLAGS=-DVALUE=2');self.assertNotEqual(obj.read_bytes(),first)
            stamp=max((repo/'build/.configuration').glob('*-*.json'),key=lambda p:p.stat().st_mtime_ns)
            changed=(obj.stat().st_mtime_ns,stamp.stat().st_mtime_ns)
            make('CFLAGS=-DVALUE=2');self.assertEqual((obj.stat().st_mtime_ns,stamp.stat().st_mtime_ns),changed)
            # Relevant Make configuration changes also invalidate the object.
            with (repo/'Makefile').open('a') as stream:stream.write('\n# configuration edit\n')
            make('CFLAGS=-DVALUE=2');self.assertEqual(len(list((repo/'build/.configuration').glob('*-*.json'))),3)
            make('CFLAGS=-DVALUE=1');self.assertEqual(obj.read_bytes(),first)
            # The publication implementation is itself a build input.
            with (repo/'scripts/atomic_output.py').open('a') as stream:stream.write('\n# publication implementation edit\n')
            make('CFLAGS=-DVALUE=1')
            self.assertEqual(len(list((repo/'build/.configuration').glob('*-*.json'))),5)
            for override in ('BUILD_DIR=src', 'TARGET=src/probe.c', 'TARGET=physics_sim', 'SHAPE_MASK_TOOL_BIN=data/tools/shape_mask_tool'):
                source = (repo/'src/probe.c').read_bytes()
                result = subprocess.run(['make', override], cwd=repo, env=FIXTURE_ENV,
                                        capture_output=True, text=True, timeout=60)
                self.assertNotEqual(result.returncode, 0, override)
                self.assertEqual((repo/'src/probe.c').read_bytes(), source)
            # An existing Make target must not bypass graph-selection admission.
            active=(repo/'build/.configuration/active.json')
            import json
            row=json.loads(active.read_text())
            selected=repo/'build/.configuration'/(row['digest']+'-'+str(row['generation'])+'.json')
            selected.write_bytes(b'unknown changed configuration')
            object_before=obj.read_bytes()
            held=subprocess.run(['make','CFLAGS=-DVALUE=1'],cwd=repo,env=FIXTURE_ENV,
                                capture_output=True,text=True,timeout=60)
            self.assertNotEqual(held.returncode,0)
            self.assertEqual(obj.read_bytes(),object_before)
            self.assertEqual(selected.read_bytes(),b'unknown changed configuration')
            active.unlink()
            held=subprocess.run(['make','CFLAGS=-DVALUE=1'],cwd=repo,env=FIXTURE_ENV,
                                capture_output=True,text=True,timeout=60)
            self.assertNotEqual(held.returncode,0)
            self.assertFalse(active.exists())


if __name__ == '__main__':unittest.main()
