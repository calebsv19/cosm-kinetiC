"""Bounded probe behavior and actual build-identity admission, including orphans."""
import os
import json
import signal
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from tool_probe import probe, capture, signal_capture_group
from unittest.mock import patch
from build_identity import compiler, checked_probe


class ToolProbe(unittest.TestCase):
    def test_capture_success_failure_and_limits(self):
        row=probe([sys.executable,'-c','print("version");print("detail",file=__import__("sys").stderr)'])
        self.assertEqual(row['status'],'passed');self.assertEqual(row['stdout'],'version');self.assertEqual(row['stderr'],'detail')
        row=probe([sys.executable,'-c','raise SystemExit(3)'])
        self.assertEqual(row['status'],'failed');self.assertEqual(row['exit_code'],3)
        row=probe([sys.executable,'-c','import os;os.write(1,b"a"*100000)'],limit=256)
        self.assertEqual(row['status'],'unverified');self.assertLessEqual(len(row['stdout']),256)
        row=probe([sys.executable,'-c','import time;time.sleep(10)'],timeout=.1)
        self.assertEqual(row['status'],'unverified');self.assertIn('wall-time',row['reason'])
        with self.assertRaises(ValueError):checked_probe([sys.executable,'-c','print("x"*10000)'],limit=32)

    def test_invalid_bounds_do_not_launch(self):
        for timeout,limit in ((0,256),(float('nan'),256),(61,256),(.1,0),(.1,True)):
            with self.assertRaises(ValueError):probe(['definitely-not-a-tool'],timeout=timeout,limit=limit)

    def test_successful_parent_cannot_leave_child_with_closed_pipes(self):
        with tempfile.TemporaryDirectory() as temporary:
            marker=Path(temporary)/'orphan'
            child='import time,pathlib;time.sleep(.5);pathlib.Path('+repr(str(marker))+').touch()'
            parent='import subprocess,sys;subprocess.Popen([sys.executable,"-c",'+repr(child)+'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)'
            row=probe([sys.executable,'-c',parent],timeout=1)
            self.assertEqual(row['status'],'passed',row)
            time.sleep(.7);self.assertFalse(marker.exists())

    def test_capture_signals_live_anchor_before_reap_without_poll(self):
        real_popen=subprocess.Popen;real_killpg=os.killpg;children=[]
        def launch(*args,**kwargs):
            child=real_popen(*args,**kwargs);children.append(child)
            child.poll=lambda:self.fail('Direct anchor must not be polled/reaped before signaling')
            return child
        def kill_group(pid,number):
            self.assertIsNone(children[0].returncode)
            self.assertIsNone(os.waitid(os.P_PID,pid,os.WEXITED | os.WNOHANG | os.WNOWAIT))
            self.assertEqual(os.getpgid(pid),pid)
            return real_killpg(pid,number)
        with patch('tool_probe.subprocess.Popen',side_effect=launch),patch('tool_probe.os.killpg',side_effect=kill_group):
            row=capture([sys.executable,'-c','print("ok")'])
        self.assertEqual(row['status'],'passed',row);self.assertTrue(row['direct_child_reaped'])
        self.assertTrue(row['terminal_processes_verified']);self.assertFalse(row['all_external_descendants_verified_terminal'])

    def test_lost_group_anchor_never_signals_persisted_number(self):
        from types import SimpleNamespace
        child=SimpleNamespace(pid=123,returncode=0)
        with patch('tool_probe.os.killpg') as kill:
            with self.assertRaises(ValueError):signal_capture_group(child,signal.SIGKILL)
            kill.assert_not_called()
        child.returncode=None
        with patch('tool_probe.os.waitid',side_effect=ChildProcessError('lost')),patch('tool_probe.os.killpg') as kill:
            with self.assertRaises(ChildProcessError):signal_capture_group(child,signal.SIGKILL)
            kill.assert_not_called()

    def test_capture_anchor_lifeline_loss_stops_owned_group(self):
        with tempfile.TemporaryDirectory() as temporary:
            marker=Path(temporary)/'orphan'
            result_read,result_write=os.pipe();life_read,life_write=os.pipe()
            command=[sys.executable,'-c','import time,pathlib;time.sleep(.6);pathlib.Path('+repr(str(marker))+').touch()']
            process=subprocess.Popen([sys.executable,'-B',str(ROOT/'scripts/tool_probe.py'),'--capture-anchor',str(result_write),str(life_read),'[]',json.dumps(command)],pass_fds=(result_write,life_read),start_new_session=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            os.close(result_write);os.close(life_read)
            try:
                time.sleep(.15);os.close(life_write);life_write=None
                self.assertEqual(process.wait(timeout=5),-signal.SIGKILL)
                time.sleep(.7);self.assertFalse(marker.exists())
            finally:
                if life_write is not None:os.close(life_write)
                os.close(result_read)
                if process.poll() is None:process.kill();process.wait()

    def test_result_protocol_and_launch_failure_hold(self):
        real_popen=subprocess.Popen
        for payload in (b'x'*17,b'bad\n',b'999\n'):
            def launch(command,**kwargs):
                code='import os,signal;os.write('+str(command[4])+','+repr(payload)+');os.close(1);os.close(2);signal.pause()'
                return real_popen([sys.executable,'-c',code],**kwargs)
            with patch('tool_probe.subprocess.Popen',side_effect=launch):
                row=capture([sys.executable,'-c','print("ignored")'])
            self.assertEqual(row['status'],'unverified');self.assertTrue(row['direct_child_reaped']);self.assertFalse(row['terminal_processes_verified'])
        row=capture(['/definitely/missing/tool'])
        self.assertEqual(row['status'],'unverified');self.assertIn('launch failed errno',row['reason'])

    def test_compiler_identity_refuses_executable_drift(self):
        with tempfile.TemporaryDirectory() as temporary:
            selected=Path(temporary)/'compiler'
            selected.write_text('#!'+sys.executable+'\nimport pathlib\nprint("identity")\npathlib.Path(__file__).open("a").write("\\n# changed\\n")\n')
            selected.chmod(0o755)
            with self.assertRaisesRegex(ValueError,'changed during identity'):compiler(str(selected))

    def test_actual_configuration_cli_holds_noisy_compiler_without_allocating_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();(repo/'scripts').mkdir()
            for name in ('tool_probe.py','build_identity.py','build_outputs.py','clean_outputs.py','check_clean_root.py','atomic_output.py','build_owner.py'):
                shutil.copy2(ROOT/'scripts'/name,repo/'scripts'/name)
            selected=repo/'compiler';selected.write_text('#!'+sys.executable+'\nprint("x"*100000)\n');selected.chmod(0o755)
            env={k:v for k,v in os.environ.items() if not k.startswith('PHYSICS_BUILD_')};env['PHYSICS_BUILD_CC']=str(selected)
            result=subprocess.run([sys.executable,'-B','scripts/build_identity.py','--digest-only','--selection-root','build/new'],
                cwd=repo,env=env,capture_output=True,text=True,timeout=5)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('output limit',result.stderr)
            self.assertFalse((repo/'build').exists());self.assertFalse((repo/'tmp').exists())


if __name__=='__main__':unittest.main()
