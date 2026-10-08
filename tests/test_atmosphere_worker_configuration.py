"""Actual worker recipes rebuild on configuration drift and hold unsafe output."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
ENV={k:v for k,v in os.environ.items() if not k.startswith('PHYSICS_BUILD_') and k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
VARS=('PASSIVE3D_WORKER','ATMOSPHERE3D_WORKER','OPEN_ATMOSPHERE3D_WORKER')

class WorkerConfiguration(unittest.TestCase):
    def test_actual_recipes_configuration_noop_rebuild_and_admission(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();(repo/'scripts').mkdir();(repo/'make').mkdir()
            for name in ('tool_probe.py','build_identity.py','build_outputs.py','clean_outputs.py','check_clean_root.py','atomic_output.py','build_owner.py'):
                shutil.copy2(ROOT/'scripts'/name,repo/'scripts'/name)
            shutil.copy2(ROOT/'make/build-identity.mk',repo/'make/build-identity.mk')
            rules=[];active=False
            for line in (ROOT/'make/rules-tools.mk').read_text().splitlines():
                if any(line.startswith(v+' :=') for v in VARS):rules.append(line)
                if line and not line.startswith(('\t',' ','#')):
                    active=any(line.startswith('$('+v+'):') for v in VARS)
                    if active:
                        rules.append(line)
                        for value in line.split(':',1)[1].split():
                            if value.startswith(('src/','include/')):
                                path=repo/value;path.parent.mkdir(parents=True,exist_ok=True)
                                path.write_text('int main(void){return VALUE;}\n' if value.endswith('.c') else '')
                elif active:rules.append(line)
            (repo/'Makefile').write_text('BUILD_DIR=build/profile\nCC=clang\nCLANG=clang\nJSON_CFLAGS ?= -DVALUE=3\nall: $(PASSIVE3D_WORKER) $(ATMOSPHERE3D_WORKER) $(OPEN_ATMOSPHERE3D_WORKER)\n'+'\n'.join(rules)+'\ninclude make/build-identity.mk\n')
            # Define aliases after actual variables have been parsed.
            with (repo/'Makefile').open('a') as stream:stream.write('\nall: $(PASSIVE3D_WORKER) $(ATMOSPHERE3D_WORKER) $(OPEN_ATMOSPHERE3D_WORKER)\n')
            def make(*args):return subprocess.run(['make','all',*args],cwd=repo,env=ENV,capture_output=True,text=True,timeout=60)
            paths=[repo/'build/profile'/p for p in ('passive-atmosphere/physics_sim_passive_worker','evolving-atmosphere/physics_sim_atmosphere_worker','open-atmosphere/physics_sim_open_atmosphere_worker')]
            def snapshot():return [(p.read_bytes(),p.stat().st_mtime_ns) for p in paths]
            r=make();self.assertEqual(r.returncode,0,r.stdout+r.stderr);first=snapshot()
            for p in paths:self.assertEqual(subprocess.run([str(p)]).returncode,3)
            r=make();self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(snapshot(),first)
            r=make('JSON_CFLAGS=-DVALUE=7');self.assertEqual(r.returncode,0,r.stderr)
            changed=snapshot();self.assertTrue(all(a[0]!=b[0] for a,b in zip(first,changed)))
            for p in paths:self.assertEqual(subprocess.run([str(p)]).returncode,7)
            r=make('JSON_CFLAGS=-DVALUE=7');self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(snapshot(),changed)
            r=make('JSON_CFLAGS=-DVALUE=7','JSON_LIBS=-lm');self.assertEqual(r.returncode,0,r.stderr);linked=snapshot()
            self.assertTrue(all(a[1]!=b[1] for a,b in zip(changed,linked)))
            r=make('JSON_CFLAGS=-DVALUE=7','JSON_LIBS=-lm');self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(snapshot(),linked)
            r=make('JSON_CFLAGS=-DVALUE=7','JSON_LIBS=-lm','CC=clang -DOTHER=1');self.assertEqual(r.returncode,0,r.stderr)
            selected=snapshot();self.assertTrue(all(a[1]!=b[1] for a,b in zip(linked,selected)))
            for variable in VARS:
                r=make(variable+'=src/forbidden');self.assertNotEqual(r.returncode,0);self.assertFalse((repo/'src/forbidden').exists());self.assertEqual(snapshot(),selected)
            paths[0].write_bytes(b'unknown predecessor');before=snapshot()
            r=make('JSON_CFLAGS=-DVALUE=8');self.assertNotEqual(r.returncode,0);self.assertEqual(snapshot(),before)

if __name__=='__main__':unittest.main()
