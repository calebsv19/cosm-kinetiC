"""Owned CFD supervision retains diagnostics and enforces direct-command bounds."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import cfd_run_support as c

class Supervision(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve()
    def execute(self,code,**options):
        return c.execute([sys.executable,'-c',code],self.root,'run',options.pop('wall_cap',10),options.pop('rss_cap',512*1024**2),**options)
    def test_success_measures_direct_command_and_retains_logs(self):
        r=self.execute("import time;print('retained');time.sleep(.2)")
        self.assertEqual(r['exit_code'],0)
        self.assertGreater(r['peak_sampled_child_rss_bytes'],0)
        self.assertTrue(r['direct_child_reaped']);self.assertTrue(r['group_cleanup_identity_anchored'])
        self.assertTrue(r['terminal_processes_verified']);self.assertFalse(r['all_external_descendants_verified_terminal'])
        self.assertIn('descendants unmeasured',r['rss_scope'])
        self.assertIn('retained',(self.root/'run.stdout').read_text())
    def test_nonzero_exit_is_observed_and_logs_retained(self):
        with self.assertRaises(c.ExecutionFailure) as ctx:self.execute("import sys;print('failure');sys.exit(7)")
        self.assertEqual(ctx.exception.exit_code,7)
        self.assertTrue((self.root/'run.stderr').exists())
        self.assertIn('failure',(self.root/'run.stdout').read_text())
    def test_rss_limit_fails_without_publishing_retained_binary(self):
        command=[sys.executable,'-c',"import pathlib,sys,time;pathlib.Path(sys.argv[-1]).write_bytes(b'candidate');a=bytearray(80*1024**2);time.sleep(2)",'-o',str(self.root/'probe')]
        with self.assertRaisesRegex(ValueError,'RSS cap'):c.compile_probe(command,self.root,'compile',10,32*1024**2)
        self.assertFalse((self.root/'probe').exists())
        attempt=next((self.root/'.compiler-attempts').iterdir())
        self.assertEqual((attempt/'probe').read_bytes(),b'candidate')
        import json
        r=json.loads((attempt/'receipt.json').read_text())
        self.assertEqual(r['status'],'failed');self.assertGreater(r['peak_sampled_child_rss_bytes'],32*1024**2)
        self.assertFalse(r['all_external_descendants_verified_terminal'])
    def test_log_limit_retains_failed_output(self):
        with self.assertRaisesRegex(ValueError,'log cap'):self.execute("import time;print('x'*8192,flush=True);time.sleep(2)",log_cap=64)
        self.assertGreater((self.root/'run.stdout').stat().st_size,64)
    def test_wall_limit_stops_work_and_retains_logs(self):
        with self.assertRaisesRegex(ValueError,'wall cap'):self.execute("import time,pathlib;time.sleep(1);pathlib.Path('unexpected').touch()",wall_cap=.1)
        time.sleep(1.1)
        self.assertFalse((self.root/'unexpected').exists());self.assertTrue((self.root/'run.stderr').exists())
    def test_reaped_group_identity_is_never_signaled(self):
        class Reaped:returncode=0;pid=12345
        with patch.object(c.os,'killpg',side_effect=AssertionError('saved group signaled')):
            with self.assertRaises(ValueError):c.signal_owned_group(Reaped(),signal.SIGTERM)
    def test_invalid_tag_and_bounds_refuse_before_files_or_launch(self):
        for tag,wall,rss,log in (('../escape',1,1,1),('run',0,1,1),('run',float('nan'),1,1),('run',1,True,1),('run',1,1,0)):
            with self.subTest(tag=tag,wall=wall,rss=rss,log=log),patch.object(c.subprocess,'Popen',side_effect=AssertionError('invalid input launched')):
                with self.assertRaises(ValueError):c.execute(['missing'],self.root,tag,wall,rss,log)
        self.assertEqual(list(self.root.iterdir()),[])
    def test_parent_loss_stops_same_group_writer(self):
        wrapper="import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);from cfd_run_support import execute;execute([sys.executable,'-c',\"import pathlib,time;pathlib.Path('ready').touch();time.sleep(1.5);pathlib.Path('unexpected-parent-loss').touch()\"],Path.cwd(),'run',10,512*1024**2)"
        p=subprocess.Popen([sys.executable,'-B','-c',wrapper,str(ROOT/'scripts')],cwd=self.root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            deadline=time.monotonic()+5
            while not (self.root/'ready').exists() and time.monotonic()<deadline:time.sleep(.01)
            self.assertTrue((self.root/'ready').exists());p.kill();p.wait(timeout=5);time.sleep(1.7)
            self.assertFalse((self.root/'unexpected-parent-loss').exists());self.assertTrue((self.root/'run.stdout').exists())
        finally:
            if p.poll() is None:p.kill();p.wait(timeout=5)

if __name__=='__main__':unittest.main()
