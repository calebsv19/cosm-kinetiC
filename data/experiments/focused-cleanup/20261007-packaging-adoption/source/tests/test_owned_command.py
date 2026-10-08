"""Retained command cleanup keeps live identity and nested shutdown opportunity."""
import json
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
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from owned_command import execute,signal_owned_group,ExecutionFailure,IncompleteTeardown

class OwnedCommand(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve()
    def test_success_failure_and_preserved_logs(self):
        row=execute([sys.executable,'-c','print("ok")'],self.root,self.root,'success',(),2,1024)
        self.assertTrue(row['direct_child_reaped']);self.assertTrue(row['group_cleanup_identity_anchored']);self.assertFalse(row['all_external_descendants_verified_terminal'])
        self.assertEqual((self.root/'success.stdout').read_text(),'ok\n')
        with self.assertRaises(ExecutionFailure) as failure:execute([sys.executable,'-c','print("partial");raise SystemExit(3)'],self.root,self.root,'failure',(),2,1024)
        self.assertEqual(failure.exception.exit_code,3);self.assertEqual((self.root/'failure.stdout').read_text(),'partial\n')
    def test_signal_precedes_reap_and_direct_poll_is_forbidden(self):
        real_popen=subprocess.Popen;real_kill=os.killpg;children=[]
        def launch(*args,**kwargs):
            child=real_popen(*args,**kwargs);children.append(child);child.poll=lambda:self.fail('Direct anchor polled before signaling');return child
        def kill(pid,number):
            self.assertIsNone(children[0].returncode);self.assertIsNone(os.waitid(os.P_PID,pid,os.WEXITED | os.WNOHANG | os.WNOWAIT));return real_kill(pid,number)
        with patch('owned_command.subprocess.Popen',side_effect=launch),patch('owned_command.os.killpg',side_effect=kill):
            execute([sys.executable,'-c','print("ok")'],self.root,self.root,'order',(),2,1024)
    def test_lost_anchor_never_signals_saved_group(self):
        from types import SimpleNamespace
        child=SimpleNamespace(pid=123,returncode=0)
        with patch('owned_command.os.killpg') as kill:
            with self.assertRaises(ValueError):signal_owned_group(child,signal.SIGKILL)
            kill.assert_not_called()
        child.returncode=None
        with patch('owned_command.os.waitid',side_effect=ChildProcessError('lost')),patch('owned_command.os.killpg') as kill:
            with self.assertRaises(ChildProcessError):signal_owned_group(child,signal.SIGKILL)
            kill.assert_not_called()
    def test_timeout_gives_nested_supervisor_shutdown_window(self):
        marker=self.root/'graceful'
        code='import signal,time,pathlib,sys;signal.signal(signal.SIGTERM,lambda *_:(pathlib.Path('+repr(str(marker))+').touch(),sys.exit(0)));print("ready",flush=True);time.sleep(30)'
        with self.assertRaisesRegex(ValueError,'wall cap'):execute([sys.executable,'-c',code],self.root,self.root,'grace',(),.3,1024)
        self.assertTrue(marker.exists());self.assertIn('ready',(self.root/'grace.stdout').read_text())
    def test_parent_lifeline_loss_terminates_current_group(self):
        result_read,result_write=os.pipe();life_read,life_write=os.pipe();marker=self.root/'orphan'
        command=[sys.executable,'-c','import time,pathlib;time.sleep(.7);pathlib.Path('+repr(str(marker))+').touch()']
        process=subprocess.Popen([sys.executable,'-B',str(ROOT/'scripts/agent_session/owned_command.py'),'--command-anchor',str(result_write),str(life_read),'[]',json.dumps(command)],cwd=self.root,pass_fds=(result_write,life_read),start_new_session=True)
        os.close(result_write);os.close(life_read)
        try:
            time.sleep(.15);os.close(life_write);life_write=None
            self.assertEqual(process.wait(timeout=5),-signal.SIGKILL);time.sleep(.8);self.assertFalse(marker.exists())
        finally:
            if life_write is not None:os.close(life_write)
            os.close(result_read)
            if process.poll() is None:process.kill();process.wait()
    def test_refused_group_cleanup_retains_held_logs(self):
        with patch('owned_command.os.killpg',side_effect=PermissionError('controlled refusal')):
            with self.assertRaises(IncompleteTeardown):execute([sys.executable,'-c','print("kept")'],self.root,self.root,'refusal',(),2,1024)
        self.assertEqual((self.root/'refusal.stdout').read_text(),'kept\n')
    def test_invalid_result_protocol_never_accepts_execution(self):
        real_popen=subprocess.Popen
        for index,payload in enumerate((b'x'*17,b'bad\n',b'999\n')):
            def launch(command,**kwargs):
                code='import os,signal;os.write('+str(command[4])+','+repr(payload)+');signal.signal(signal.SIGTERM,lambda *_:None);signal.pause()'
                return real_popen([sys.executable,'-c',code],**kwargs)
            with patch('owned_command.subprocess.Popen',side_effect=launch):
                with self.assertRaises(IncompleteTeardown):execute([sys.executable,'-c','print("ignored")'],self.root,self.root,'protocol'+str(index),(),2,1024)

    def test_bad_tags_and_bounds_hold_before_logs_or_launch(self):
        for tag,wall,cap in (('../escape',1,1024),('ok',float('nan'),1024),('ok',0,1024),('ok',1,True)):
            with patch('owned_command.subprocess.Popen') as launch:
                with self.assertRaises(ValueError):execute(['not-a-command'],self.root,self.root,tag,(),wall,cap)
                launch.assert_not_called()
        self.assertEqual(list(self.root.iterdir()),[])

if __name__=='__main__':unittest.main()
