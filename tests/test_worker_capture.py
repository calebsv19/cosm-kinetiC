"""Live native-output limits, descriptor inheritance and owned process cleanup."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from tool_probe import capture
from passive_atmosphere import run_atmosphere_worker

class WorkerCapture(unittest.TestCase):
    def test_raw_output_and_independent_live_limits(self):
        row=capture([sys.executable,'-c','import os;os.write(1,b" "+bytes([255])+b"\\n");os.write(2,b"err")'],stdout_limit=8,stderr_limit=3)
        self.assertEqual(row['status'],'passed');self.assertEqual(row['stdout'],b' \xff\n');self.assertEqual(row['stderr'],b'err')
        for fd,key in ((1,'stdout'),(2,'stderr')):
            row=capture([sys.executable,'-c',f'import os,time;os.write({fd},b"x"*10000);time.sleep(30)'],stdout_limit=256,stderr_limit=64,timeout=1)
            self.assertEqual(row['status'],'unverified');self.assertIn('output limit',row['reason']);self.assertLessEqual(len(row[key]),256 if fd==1 else 64)
    def test_timeout_closed_pipes_and_background_teardown(self):
        row=capture([sys.executable,'-c','import os,time;os.close(1);os.close(2);time.sleep(30)'],timeout=.1)
        self.assertEqual(row['status'],'unverified')
        with tempfile.TemporaryDirectory() as tmp:
            marker=Path(tmp)/'orphan'
            child='import time,pathlib;time.sleep(.4);pathlib.Path('+repr(str(marker))+').touch()'
            parent='import subprocess,sys;subprocess.Popen([sys.executable,"-c",'+repr(child)+'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)'
            self.assertEqual(capture([sys.executable,'-c',parent])['status'],'passed')
            time.sleep(.6);self.assertFalse(marker.exists())
    def test_owned_descriptor_passes_to_child(self):
        with tempfile.TemporaryFile() as f:
            row=capture([sys.executable,'-c',f'import os;print(os.fstat({f.fileno()}).st_ino)'],pass_fds=(f.fileno(),))
            self.assertEqual(row['status'],'passed');self.assertEqual(int(row['stdout']),os.fstat(f.fileno()).st_ino)
    def test_bounds_and_worker_adapter_fail_closed(self):
        for kwargs in ({'stdout_limit':0},{'stderr_limit':True},{'timeout':121},{'stdout_limit':268435457}):
            with self.assertRaises(ValueError):capture(['not-a-worker'],**kwargs)
        with tempfile.TemporaryDirectory() as tmp:
            worker=Path(tmp)/'worker';worker.write_text('#!'+sys.executable+'\nimport os,time\nos.write(1,b"x"*10000)\ntime.sleep(30)\n');worker.chmod(0o755)
            with self.assertRaisesRegex(ValueError,'output limit'):run_atmosphere_worker(worker,Path(tmp)/'request',32)
    def test_teardown_refusal_holds_success(self):
        with patch('tool_probe.os.killpg',side_effect=PermissionError('controlled refusal')):
            row=capture([sys.executable,'-c','print("ok")'])
        self.assertEqual(row['status'],'unverified');self.assertIn('teardown unverified',row['reason'])
    def test_sigterm_does_not_leave_accepted_result_or_child(self):
        with tempfile.TemporaryDirectory() as tmp:
            ready=Path(tmp)/'ready';marker=Path(tmp)/'orphan';result=Path(tmp)/'result'
            child='import pathlib,time;pathlib.Path('+repr(str(ready))+').touch();time.sleep(.8);pathlib.Path('+repr(str(marker))+').touch()'
            code='import sys,json,pathlib;sys.path.insert(0,'+repr(str(ROOT/'scripts'))+');from tool_probe import capture;r=capture([sys.executable,"-c",'+repr(child)+']);pathlib.Path('+repr(str(result))+').write_text(r["status"])'
            process=subprocess.Popen([sys.executable,'-B','-c',code])
            try:
                deadline=time.monotonic()+3
                while not ready.exists() and time.monotonic()<deadline:time.sleep(.01)
                self.assertTrue(ready.exists());process.send_signal(signal.SIGTERM);process.wait(timeout=3)
                self.assertEqual(result.read_text(),'unverified');time.sleep(.9);self.assertFalse(marker.exists())
            finally:
                if process.poll() is None:process.kill();process.wait()

if __name__=='__main__':unittest.main()
