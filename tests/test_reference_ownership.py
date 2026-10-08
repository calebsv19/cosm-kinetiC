"""Kernel namespace ownership and descriptor inheritance for reference writers."""
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import cfd_run_support as c
import build_owner as b

CHECK="""import sys
from pathlib import Path
sys.path.insert(0,sys.argv[1]);import cfd_run_support as c
try:
 with c.reference_ownership(Path(sys.argv[2]),Path(sys.argv[3])):pass
except ValueError as error:print(str(error));raise SystemExit(2)
raise SystemExit(0)
"""
class Ownership(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.repo=Path(self.tmp.name).resolve();self.data=self.repo/'build/c3d-test/runs'
    def check(self,data):
        env=dict(os.environ);env.pop(c.REFERENCE_OWNER_ENV,None)
        return subprocess.run([sys.executable,'-B','-c',CHECK,str(ROOT/'scripts'),str(self.repo),str(data)],env=env,capture_output=True,text=True,timeout=5)
    def test_same_namespace_and_cleanup_are_excluded_siblings_independent(self):
        with c.reference_ownership(self.repo,self.data):
            self.assertEqual(self.check(self.data).returncode,2)
            self.assertEqual(self.check(self.repo/'build/c3d-test/other-runs').returncode,2)
            self.assertEqual(self.check(self.repo/'build/c3d-sibling/runs').returncode,0)
            with (self.repo/'tmp/locks/clean.lock').open('r+b') as f:
                with self.assertRaises(BlockingIOError):fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        self.assertEqual(self.check(self.data).returncode,0)
    def test_build_hierarchy_is_the_existing_owning_convention(self):
        selected,paths=c.reference_owner_paths(self.repo,self.data)
        self.assertEqual(paths,b.ownership_paths(self.repo,selected))
        fds=b.acquire(self.repo,self.repo/'build')
        try:self.assertEqual(self.check(self.data).returncode,2)
        finally:
            for fd in fds:os.close(fd)
    def test_locked_or_special_lock_storage_holds_without_run_allocation(self):
        locks=self.repo/'tmp/locks';locks.mkdir(parents=True)
        fifo=locks/'clean.lock';os.mkfifo(fifo)
        with self.assertRaises(ValueError):
            with c.reference_ownership(self.repo,self.data):pass
        self.assertFalse(self.data.exists());fifo.unlink()
        target=self.repo/'outside';target.write_bytes(b'keep');fifo.symlink_to(target)
        with self.assertRaises(ValueError):
            with c.reference_ownership(self.repo,self.data):pass
        self.assertEqual(target.read_bytes(),b'keep');fifo.unlink()
        with (locks/'clean.lock').open('w+b') as f:
            fcntl.flock(f,fcntl.LOCK_EX)
            self.assertEqual(self.check(self.data).returncode,2)
    def test_nested_borrow_and_forged_descriptor_metadata(self):
        with c.reference_ownership(self.repo,self.data) as fds:
            with c.reference_ownership(self.repo,self.data) as nested:self.assertEqual(nested,fds)
            value=json.loads(os.environ[c.REFERENCE_OWNER_ENV]);saved=os.environ[c.REFERENCE_OWNER_ENV]
            unrelated=self.repo/'unrelated';unrelated.write_bytes(b'')
            for path in (unrelated,c.reference_serial_paths(self.repo,self.data)[-1]):
                fd=os.open(path,os.O_RDWR)
                try:
                    value['fds'][-1]=fd;os.environ[c.REFERENCE_OWNER_ENV]=json.dumps(value)
                    with self.assertRaises(ValueError):c.reference_owner_descriptors(self.data)
                finally:os.close(fd);os.environ[c.REFERENCE_OWNER_ENV]=saved
        self.assertNotIn(c.REFERENCE_OWNER_ENV,os.environ)
    def test_inherited_child_keeps_namespace_held_after_parent_context_closes(self):
        child=None
        try:
            with c.reference_ownership(self.repo,self.data) as fds:
                child=subprocess.Popen([sys.executable,'-B','-c','import sys;sys.stdin.buffer.read(1)'],stdin=subprocess.PIPE,pass_fds=fds)
            self.assertIsNone(child.poll());self.assertEqual(self.check(self.data).returncode,2)
            child.communicate(b'x',timeout=5);self.assertEqual(child.returncode,0)
            self.assertEqual(self.check(self.data).returncode,0)
        finally:
            if child is not None and child.poll() is None:child.kill();child.wait(timeout=5)
    def test_actual_supervised_command_inherits_verified_reference_descriptors(self):
        code="""import os,json,sys
sys.path.insert(0,sys.argv[1]);import cfd_run_support as c
fds=c.reference_owner_descriptors(sys.argv[2]);assert fds
print(json.dumps({'owned_fds':len(fds)}))
"""
        with c.reference_ownership(self.repo,self.data) as fds:
            d=self.data/'run';d.mkdir(parents=True)
            result=c.execute([sys.executable,'-B','-c',code,str(ROOT/'scripts'),str(d)],d,'owned',5,512*1024**2)
            self.assertEqual(result['reference_ownership_descriptor_count'],len(fds))
            self.assertEqual(json.loads((d/'owned.stdout').read_text())['owned_fds'],len(fds))
    def test_evidence_namespace_exclusion_and_independence(self):
        data=self.repo/'data/experiments/reference/runs'
        with c.reference_ownership(self.repo,data):
            self.assertEqual(self.check(data).returncode,2)
            self.assertEqual(self.check(self.repo/'data/experiments/sibling/runs').returncode,0)

if __name__=='__main__':unittest.main()
