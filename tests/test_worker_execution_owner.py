"""Execution ownership holds the build hierarchy until worker acceptance ends."""
import fcntl
import os
import json
from unittest.mock import patch
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from build_owner import acquire,worker_execution,execution_descriptors,lock_path,ownership_paths,inherited_descriptors
from tool_probe import capture

class ExecutionOwner(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.repo=Path(self.temp.name).resolve()
        self.worker=self.repo/'build/profile/passive-atmosphere/worker';self.worker.parent.mkdir(parents=True);self.worker.write_bytes(b'worker')
    def assert_held(self,root):
        with self.assertRaisesRegex(ValueError,'held'):acquire(self.repo,self.repo/root)
    def test_full_context_blocks_cleanup_and_overlapping_build_allows_sibling(self):
        with worker_execution(self.repo,self.worker):
            self.assert_held('build');self.assert_held('build/profile');self.assert_held('build/profile/passive-atmosphere')
            other=acquire(self.repo,self.repo/'build/other')
            for fd in other:os.close(fd)
            with (self.repo/'tmp/locks/clean.lock').open('r') as lock:
                with self.assertRaises(BlockingIOError):fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            row=capture([sys.executable,'-c','import os,sys;print(all(os.fstat(int(v)) for v in sys.argv[1:]))',*map(str,execution_descriptors())],pass_fds=execution_descriptors())
            self.assertEqual(row['status'],'passed');self.assertEqual(row['stdout'],b'True\n')
            # The worker has exited; hashing/acceptance still retains ownership.
            self.assert_held('build/profile')
        other=acquire(self.repo,self.repo/'build/profile')
        for fd in other:os.close(fd)
        self.assertEqual(execution_descriptors(),())
    def test_active_clean_or_build_holds_before_body(self):
        locks=self.repo/'tmp/locks';locks.mkdir(parents=True)
        with (locks/'clean.lock').open('w') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            with self.assertRaisesRegex(ValueError,'held'):
                with worker_execution(self.repo,self.worker):self.fail('body entered')
        descriptors=acquire(self.repo,self.repo/'build/profile')
        try:
            with self.assertRaisesRegex(ValueError,'held'):
                with worker_execution(self.repo,self.worker):self.fail('body entered')
        finally:
            for fd in descriptors:os.close(fd)
    def test_external_symlink_missing_and_exception_release(self):
        outside=self.repo/'external';outside.write_bytes(b'worker')
        with self.assertRaisesRegex(ValueError,'checkout/build'):
            with worker_execution(self.repo,outside):self.fail('body entered')
        link=self.worker.parent/'linked';link.symlink_to(self.worker)
        with self.assertRaises(ValueError):
            with worker_execution(self.repo,link):self.fail('body entered')
        with self.assertRaisesRegex(ValueError,'missing'):
            with worker_execution(self.repo,self.worker.parent/'missing'):self.fail('body entered')
        with self.assertRaisesRegex(RuntimeError,'controlled'):
            with worker_execution(self.repo,self.worker):raise RuntimeError('controlled')
        descriptors=acquire(self.repo,self.repo/'build/profile')
        for fd in descriptors:os.close(fd)
    def test_unlocked_matching_descriptor_metadata_is_not_borrowed(self):
        descriptors=acquire(self.repo,self.repo/'build/profile')
        for fd in descriptors:os.close(fd)
        descriptors=[os.open(p,os.O_RDWR) for p in ownership_paths(self.repo,self.repo/'build/profile')]
        try:
            env={'PHYSICS_SIM_BUILD_OWNER_ROOT':str(self.repo/'build/profile'),
                 'PHYSICS_SIM_BUILD_HIERARCHY_FDS':json.dumps(descriptors),
                 'PHYSICS_SIM_BUILD_GLOBAL_FD':str(descriptors[0]),'PHYSICS_SIM_BUILD_ROOT_FD':str(descriptors[-1])}
            with patch.dict(os.environ,env):
                self.assertEqual(inherited_descriptors(self.repo,self.repo/'build/profile'),())
                active=acquire(self.repo,self.repo/'build/profile')
                try:self.assertEqual(inherited_descriptors(self.repo,self.repo/'build/profile'),())
                finally:
                    for fd in active:os.close(fd)
                with worker_execution(self.repo,self.worker):self.assert_held('build/profile')
        finally:
            for fd in descriptors:os.close(fd)
    def test_real_make_owner_borrowed_without_deadlock(self):
        env={k:v for k,v in os.environ.items() if not k.startswith('PHYSICS_SIM_BUILD_')}
        code='import sys;sys.path.insert(0,'+repr(str(ROOT/'scripts'))+');from pathlib import Path;from build_owner import inherited_descriptors,worker_execution,execution_descriptors;repo=Path.cwd();fds=inherited_descriptors(repo,repo/"build/profile");assert fds\nwith worker_execution(repo,repo/"build/profile/passive-atmosphere/worker"):assert execution_descriptors()==fds'
        r=subprocess.run([sys.executable,'-B',str(ROOT/'scripts/build_owner.py'),'--root','build/profile','--',sys.executable,'-B','-c',code],cwd=self.repo,env=env,capture_output=True,text=True,timeout=5)
        self.assertEqual(r.returncode,0,r.stderr)

if __name__=='__main__':unittest.main()
