"""Covering Make ownership without downgrade or concurrent recipe overlap."""
import fcntl
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import cfd_run_support as c
import build_owner as b
CHECK="""import sys,json
from pathlib import Path
sys.path.insert(0,sys.argv[1]);import cfd_run_support as c
try:
 with c.reference_ownership(Path(sys.argv[2]),Path(sys.argv[3])) as fds:
  assert fds;print('borrowed')
except ValueError as error:print(str(error));raise SystemExit(2)
"""
class Borrowing(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.repo=Path(self.tmp.name).resolve();self.data=self.repo/'build/c3d-test/runs';self.owner=self.repo/'build'
        self.fds=b.acquire(self.repo,self.owner);self.addCleanup(lambda:[os.close(fd) for fd in self.fds])
        self.env={'PHYSICS_SIM_BUILD_OWNER_ROOT':str(self.owner),'PHYSICS_SIM_BUILD_HIERARCHY_FDS':json.dumps(self.fds),'PHYSICS_SIM_BUILD_GLOBAL_FD':str(self.fds[0]),'PHYSICS_SIM_BUILD_ROOT_FD':str(self.fds[-1])}
    def check(self,data):
        env=dict(os.environ,**self.env);env.pop(c.REFERENCE_OWNER_ENV,None)
        return subprocess.run([sys.executable,'-B','-c',CHECK,str(ROOT/'scripts'),str(self.repo),str(data)],env=env,pass_fds=self.fds,capture_output=True,text=True,timeout=5)
    def assert_outer_exclusive(self):
        with b.ownership_paths(self.repo,self.owner)[-1].open('r+b') as witness:
            with self.assertRaises(BlockingIOError):fcntl.flock(witness,fcntl.LOCK_SH|fcntl.LOCK_NB)
    def test_covering_parent_borrow_keeps_mode_and_lifetime(self):
        with patch.dict(os.environ,self.env):
            with c.reference_ownership(self.repo,self.data) as fds:
                self.assertEqual(fds[:len(self.fds)],tuple(self.fds));self.assert_outer_exclusive()
                self.assertEqual(c.reference_owner_descriptors(self.data),fds)
            self.assert_outer_exclusive()
            self.assertTrue(b.inherited_descriptors(self.repo,self.owner))
    def test_same_make_recipes_exclude_same_and_overlapping_reference_scopes(self):
        with patch.dict(os.environ,self.env),c.reference_ownership(self.repo,self.data):
            self.assertEqual(self.check(self.data).returncode,2)
            self.assertEqual(self.check(self.repo/'build/c3d-test/deep/runs').returncode,2)
            self.assertEqual(self.check(self.repo/'build/c3d-sibling/runs').returncode,0)
        self.assertEqual(self.check(self.data).returncode,0);self.assert_outer_exclusive()
    def test_forged_build_alias_or_unheld_correct_inode_is_rejected(self):
        for bad in ({'PHYSICS_SIM_BUILD_ROOT_FD':'999999'}, {'PHYSICS_SIM_BUILD_HIERARCHY_FDS':'[true]'}):
            with patch.dict(os.environ,{**self.env,**bad}):
                with self.assertRaises(ValueError):
                    with c.reference_ownership(self.repo,self.data):pass
        fd=os.open(b.ownership_paths(self.repo,self.owner)[-1],os.O_RDWR)
        try:
            fake=[*self.fds[:-1],fd]
            with patch.dict(os.environ,{**self.env,'PHYSICS_SIM_BUILD_HIERARCHY_FDS':json.dumps(fake),'PHYSICS_SIM_BUILD_ROOT_FD':str(fd)}):
                with self.assertRaises(ValueError):
                    with c.reference_ownership(self.repo,self.data):pass
        finally:os.close(fd)
        self.assertFalse(self.data.exists());self.assert_outer_exclusive()
    def test_actual_parallel_make_recipes_borrow_and_serialize(self):
        # Release this test's independently acquired owner before launching the
        # real outer Make owner; use a fresh sibling checkout for that proof.
        repo=self.repo/'actual-make';repo.mkdir()
        script=repo/'recipe.py'
        script.write_text("""import sys,time,json
from pathlib import Path
sys.path.insert(0,sys.argv[1]);import cfd_run_support as c
repo=Path(sys.argv[2]);role=sys.argv[3];data=repo/'build/c3d-real/runs'
ready=repo/'ready';release=repo/'release'
if role=='hold':
 with c.reference_ownership(repo,data):
  ready.write_text('ready')
  deadline=time.monotonic()+10
  while not release.exists():
   if time.monotonic()>deadline:raise ValueError('recipe release deadline')
   time.sleep(.01)
else:
 deadline=time.monotonic()+10
 while not ready.exists():
  if time.monotonic()>deadline:raise ValueError('recipe ready deadline')
  time.sleep(.01)
 try:
  try:
   with c.reference_ownership(repo,data):raise AssertionError('overlapping recipe admitted')
  except ValueError:pass
  with c.reference_ownership(repo,repo/'build/c3d-independent/runs'):pass
  (repo/'verified').write_text('same held; sibling admitted')
 finally:release.write_text('release')
""")
        prefix=' '.join(shlex.quote(str(x)) for x in (sys.executable,'-B',script,ROOT/'scripts',repo))
        makefile=repo/'Makefile';makefile.write_text('.PHONY: all hold contend\nall: hold contend\nhold:\n\t'+prefix+' hold\ncontend:\n\t'+prefix+' contend\n')
        env=dict(os.environ)
        for key in (*self.env,c.REFERENCE_OWNER_ENV):env.pop(key,None)
        result=subprocess.run([sys.executable,'-B',str(ROOT/'scripts/build_owner.py'),'--root',str(repo/'build'),'--','make','-f',str(makefile),'-j2','all'],cwd=repo,env=env,capture_output=True,text=True,timeout=20)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual((repo/'verified').read_text(),'same held; sibling admitted')

    def test_actual_supervised_child_preserves_covering_build_and_reference_ownership(self):
        code="""import sys
sys.path.insert(0,sys.argv[1]);import cfd_run_support as c
assert c.reference_owner_descriptors(sys.argv[2]);print('verified-child')
"""
        with patch.dict(os.environ,self.env),c.reference_ownership(self.repo,self.data):
            run=self.data/'owned-run';run.mkdir(parents=True)
            c.execute([sys.executable,'-B','-c',code,str(ROOT/'scripts'),str(run)],run,'child',5,512*1024**2)
            self.assertIn('verified-child',(run/'child.stdout').read_text());self.assert_outer_exclusive()

if __name__=='__main__':unittest.main()
