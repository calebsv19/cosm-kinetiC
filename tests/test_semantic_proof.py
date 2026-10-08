"""Semantic recipes retain attempts and never overwrite their legacy hints."""
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
ENV = {k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES') and not k.startswith('PHYSICS_SIM_BUILD_')}
ENV['PYTHONDONTWRITEBYTECODE'] = '1'
sys.path.insert(0, str(ROOT/'scripts'))
from cfd_evidence import verify_bundle


class Semantic(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name).resolve()
        (self.repo/'scripts').mkdir(); (self.repo/'src').mkdir()
        (self.repo/'src/probe.c').write_text('int value(void){return 7;}\n')
        for name in ('semantic_proof.py','clean_outputs.py','build_outputs.py','build_owner.py','check_clean_root.py','cfd_evidence.py'):
            shutil.copy2(ROOT/'scripts'/name, self.repo/'scripts'/name)
        self.build = self.repo/'build/proof'; self.build.mkdir(parents=True)
        self.obj = self.build/'probe.o'; self.obj.write_bytes(b'prior object')
        self.log = self.build/'probe.sema.txt'; self.log.write_bytes(b'prior diagnostics')
        self.stub = self.repo/'compiler.py'
        self.stub.write_text("import pathlib,sys; print('semantic diagnostics');pathlib.Path(sys.argv[sys.argv.index('-o')+1]).write_bytes(b'new object')\n")

    def command(self, *extra, object_hint='build/proof/probe.o', log_hint='build/proof/probe.sema.txt'):
        return [sys.executable,'-B','scripts/semantic_proof.py','--build-root','build/proof',
                '--parent','data/experiments/semantic-proofs','--object',object_hint,'--log',log_hint,
                '--',sys.executable,str(self.stub),*extra,'--dump-sema','-c','src/probe.c','-o',object_hint]

    def invoke(self, *extra, **kwargs):
        return subprocess.run(self.command(*extra, **kwargs), cwd=self.repo, env=ENV, capture_output=True, text=True, timeout=15)

    def capsules(self):
        return sorted((self.repo/'data/experiments/semantic-proofs').glob('semantic-*'))

    def preserved(self):
        self.assertEqual(self.obj.read_bytes(), b'prior object')
        self.assertEqual(self.log.read_bytes(), b'prior diagnostics')

    def test_success_and_failure_create_separate_verified_evidence(self):
        good = self.invoke(); self.assertEqual(good.returncode,0,good.stderr)
        first = self.capsules()[0]; before = (first/'bundle_manifest.json').read_bytes()
        self.stub.write_text("import pathlib,sys;print('failure diagnostics');pathlib.Path(sys.argv[-1]).write_bytes(b'partial');sys.exit(7)\n")
        bad = self.invoke(); self.assertEqual(bad.returncode,7,bad.stderr)
        self.assertEqual(len(self.capsules()),2); self.preserved()
        self.assertEqual((first/'bundle_manifest.json').read_bytes(),before)
        for capsule in self.capsules(): verify_bundle(capsule)
        rows = [json.loads((p/'receipt.json').read_text()) for p in self.capsules()]
        self.assertEqual({p['status'] for p in rows},{'passed','failed'})
        self.assertTrue(all(not p['legacy_outputs_modified'] for p in rows))

    def test_concurrent_direct_commands_observe_build_ownership(self):
        self.stub.write_text("import pathlib,sys,time;pathlib.Path('ready').touch();time.sleep(30)\n")
        process = subprocess.Popen(self.command(),cwd=self.repo,env=ENV,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic()+5
            while not (self.repo/'ready').exists() and process.poll() is None and time.monotonic()<deadline: time.sleep(.01)
            self.assertTrue((self.repo/'ready').exists())
            blocked = self.invoke(); self.assertEqual(blocked.returncode,2); self.assertIn('held',blocked.stderr)
            self.assertEqual(len(self.capsules()),1)
            process.terminate(); self.assertEqual(process.wait(timeout=5),143)
            row=json.loads((self.capsules()[0]/'receipt.json').read_text());self.assertEqual(row['status'],'failed')
            verify_bundle(self.capsules()[0]); self.preserved()
        finally:
            if process.poll() is None: process.kill();process.wait()

    def test_source_escape_and_symlink_hints_refuse_before_compilation(self):
        for obj,log in (('src/probe.c','build/proof/probe.sema.txt'),('build/proof/probe.o','src/probe.c')):
            result=self.invoke(object_hint=obj,log_hint=log)
            self.assertNotEqual(result.returncode,0);self.preserved()
        self.obj.unlink();self.obj.symlink_to(self.repo/'src/probe.c')
        result=self.invoke();self.assertNotEqual(result.returncode,0)
        self.assertEqual((self.repo/'src/probe.c').read_text(),'int value(void){return 7;}\n')

    def test_success_without_object_or_source_drift_is_failed(self):
        self.stub.write_text("print('no object')\n")
        self.assertEqual(self.invoke().returncode,2)
        self.stub.write_text("import pathlib,sys;pathlib.Path(sys.argv[-1]).write_bytes(b'object');pathlib.Path('src/probe.c').write_text('changed')\n")
        self.assertEqual(self.invoke().returncode,2)
        for capsule in self.capsules():
            self.assertEqual(json.loads((capsule/'receipt.json').read_text())['status'],'failed');verify_bundle(capsule)
        self.preserved()

    def test_all_eighteen_actual_make_recipes_route_to_retained_capsules(self):
        source=(ROOT/'make/rules-build.mk').read_text()
        blocks=re.findall(r'(dump-sema[^:\n]*): ([^\n]+)\n\t(FISICS_MAX_PROCS=0 python3 -B scripts/semantic_proof.py[^\n]+)',source)
        self.assertEqual(len(blocks),18)
        lines=['BUILD_DIR=build/proof','EXPERIMENT_DIR=data/experiments', 'FISICS='+sys.executable+' '+str(self.stub), 'FISICS_FLAGS=', 'FISICS_CFLAGS=']
        variables={name for _,deps,recipe in blocks for name in re.findall(r'\$\((SEMA_[^)]+)\)',deps+' '+recipe)}
        for name in variables:
            value='src/probe.c' if name.endswith('SRC') else 'build/proof/probe.o' if name.endswith('OBJ') else 'build/proof/probe.sema.txt'
            lines.append(name+'='+value)
        lines += [goal+': '+deps+'\n\t'+recipe for goal,deps,recipe in blocks]
        (self.repo/'Makefile').write_text('\n'.join(lines)+'\n')
        result=subprocess.run(['make',*[goal for goal,_,_ in blocks]],cwd=self.repo,env=ENV,capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual(len(self.capsules()),18);self.preserved()
        for capsule in self.capsules():verify_bundle(capsule)


if __name__ == '__main__':unittest.main()
