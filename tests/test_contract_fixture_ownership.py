"""Actual Make fixture recipes preserve predecessors and emit cleanup ownership."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from build_outputs import inventory
from clean_outputs import plan,apply


def recipe_bodies():
    source=Path(os.environ.get('PHYSICS_FIXTURE_RULES',ROOT/'make/rules-tests-contracts.mk')).read_text()
    rows={};target=None
    for line in source.splitlines():
        match=re.match(r'^([a-z][a-z0-9_-]*):(?:\s|$)',line)
        if match:
            target=match[1];rows[target]=[]
        elif line.startswith('\t') and target:
            rows[target].append(line)
        elif line and not line.startswith('#'):
            target=None
    return {target:body for target,body in rows.items() if any('$(CC)' in line for line in body)}


class FixtureOwnership(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup)
        self.repo=Path(self.t.name).resolve();(self.repo/'scripts').mkdir()
        for name in ('atomic_output.py','build_owner.py','build_outputs.py','clean_outputs.py','check_clean_root.py'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        self.rows=recipe_bodies();self.assertTrue(self.rows)
        self.target='test-scene-project-cache-output-status-contract'
        compiler=self.repo/'compiler.py'
        compiler.write_text("import pathlib,sys\npathlib.Path('calls').open('a').write('call\\n')\nout=pathlib.Path(sys.argv[sys.argv.index('-o')+1]);out.write_text('#!/bin/sh\\nexit 0\\n');out.chmod(0o755)\nraise SystemExit(1 if pathlib.Path('fail').exists() else 0)\n")
        headers=['BUILD_DIR=build/profile','CC='+sys.executable+' -B compiler.py']
        for target,body in self.rows.items():headers+= [target+':',*body]
        (self.repo/'Makefile').write_text('\n'.join(headers)+'\n')
        self.env={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES','PHYSICS_SIM_BUILD_OWNER_ROOT','PHYSICS_SIM_BUILD_OWNER_FDS')}
    def make(self,*targets):
        return subprocess.run(['make',*targets],cwd=self.repo,env=self.env,capture_output=True,text=True,timeout=60)
    def test_every_selected_recipe_records_exact_disposable_outputs(self):
        result=self.make(*self.rows);self.assertEqual(result.returncode,0,result.stderr)
        files=inventory(self.repo,self.repo/'build/profile')
        self.assertEqual(len(files),len(self.rows))
        protected=[self.repo/'data',self.repo/'tmp/tests',self.repo/'src']
        planned=plan(self.repo,self.repo/'build/profile',protected,[])
        self.assertEqual(len(planned['owned_files']),len(self.rows))
        # Apply only inside this owned temporary fixture, never the user worktree.
        result=apply(planned,lambda:plan(self.repo,self.repo/'build/profile',protected,[]))
        self.assertFalse((self.repo/'build/profile').exists());self.assertTrue(result['removed'])
    def test_failed_rebuild_preserves_published_fixture_and_receipt(self):
        first=self.make(self.target);self.assertEqual(first.returncode,0,first.stderr)
        output=self.repo/'build/profile/scene_project_cache_output_status_contract_test'
        before=output.read_bytes();owned=inventory(self.repo,output.parent)
        (self.repo/'fail').touch();failed=self.make(self.target);self.assertNotEqual(failed.returncode,0)
        self.assertEqual(output.read_bytes(),before);self.assertEqual(inventory(self.repo,output.parent),owned)
    def test_unknown_predecessor_holds_before_compiler_invocation(self):
        output=self.repo/'build/profile/scene_project_cache_output_status_contract_test';output.parent.mkdir(parents=True);output.write_bytes(b'valuable unknown output')
        result=self.make(self.target);self.assertNotEqual(result.returncode,0)
        self.assertFalse((self.repo/'calls').exists());self.assertEqual(output.read_bytes(),b'valuable unknown output')

if __name__=='__main__':unittest.main()
