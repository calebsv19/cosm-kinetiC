"""Fixture supervision binds terminal outcomes without authorizing retirement."""
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from fixture_session import supervise
from unittest.mock import patch


class FixtureSession(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve()
        for folder in ('scripts','tests/integration'):(self.repo/folder).mkdir(parents=True)
        for name in ('fixture_session.py','fixture_root.py','clean_outputs.py','build_outputs.py','check_clean_root.py','build_owner.py','cfd_evidence.py'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        shutil.copy2(ROOT/'tests/integration/fixture_support.sh',self.repo/'tests/integration/fixture_support.sh')
        self.script=self.repo/'tests/integration/run_controlled.sh'
        self.prefix='#!/usr/bin/env bash\nset -euo pipefail\nR="'+str(self.repo)+'"\nsource "$R/tests/integration/fixture_support.sh"\nphysics_fixture_supervise "$R" "$0" "$@"\nO="$(physics_fixture_root "$R" controlled)"\necho "$O" > "$R/selected-root"\n'
        self.env={k:v for k,v in os.environ.items() if not k.startswith('PHYSICS_SIM_') and k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}

    def invoke(self,body):
        self.script.write_text(self.prefix+body)
        return subprocess.run(['bash',str(self.script)],cwd=self.repo,env=self.env,capture_output=True,text=True,timeout=15)

    def terminal(self):
        root=Path((self.repo/'selected-root').read_text().strip())
        return root,json.loads((root.parent/'fixture_terminal.json').read_text())

    def test_success_and_failure_reruns_have_distinct_bound_terminal_receipts(self):
        result=self.invoke('echo complete > "$O/output"\n');self.assertEqual(result.returncode,0,result.stderr)
        first,terminal=self.terminal();self.assertEqual(terminal['status'],'passed')
        self.assertFalse(terminal['owned_process_group_reaped']);self.assertTrue(terminal['direct_child_reaped']);self.assertTrue(terminal['owned_process_group_termination_requested'] or terminal['owned_process_group_absent_at_cleanup']);self.assertFalse(terminal['payload_integrity_verified'])
        self.assertFalse(terminal['pruning_authorized'])
        result=self.invoke('echo partial > "$O/output"\nexit 7\n');self.assertEqual(result.returncode,7,result.stderr)
        second,terminal=self.terminal();self.assertNotEqual(first,second);self.assertEqual(terminal['status'],'failed')
        self.assertEqual((first/'output').read_text().strip(),'complete');self.assertEqual((second/'output').read_text().strip(),'partial')
        owner=json.loads((second.parent/'fixture_owner.json').read_text());self.assertEqual(owner['session_id'],terminal['session_id'])

    def test_active_fixture_holds_kernel_lock_then_records_signal_failure(self):
        self.script.write_text(self.prefix+'echo ready > "$R/ready"\nsleep 20\n')
        process=subprocess.Popen(['bash',str(self.script)],cwd=self.repo,env=self.env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,text=True)
        try:
            deadline=time.monotonic()+5
            while not (self.repo/'ready').exists() and time.monotonic()<deadline:time.sleep(.02)
            self.assertTrue((self.repo/'ready').exists())
            root=Path((self.repo/'selected-root').read_text().strip());owner=json.loads((root.parent/'fixture_owner.json').read_text())
            self.assertFalse((root.parent/'fixture_terminal.json').exists())
            import fcntl
            with (Path(owner['session_dir'])/'owner.lock').open('rb') as lock:
                with self.assertRaises(BlockingIOError):fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            process.send_signal(signal.SIGTERM);_,errors=process.communicate(timeout=5)
            self.assertTrue((root.parent/'fixture_terminal.json').exists(),errors)
            _,terminal=self.terminal();self.assertEqual(terminal['status'],'failed')
        finally:
            if process.poll() is None:process.kill();process.wait()

    def test_missing_descriptor_cannot_forge_active_session(self):
        self.script.write_text(self.prefix+'echo done > "$O/output"\n')
        env=dict(self.env,PHYSICS_SIM_FIXTURE_SESSION_DIR=str(self.repo/'tmp/fixture-sessions'/('0'*32)),PHYSICS_SIM_FIXTURE_SESSION_FD='123456')
        result=subprocess.run(['bash',str(self.script)],cwd=self.repo,env=env,capture_output=True,text=True,timeout=15)
        self.assertEqual(result.returncode,0,result.stderr)
        _,terminal=self.terminal();self.assertNotEqual(terminal['session_id'],'0'*32)

    def test_successful_fixture_reaps_same_group_background_child(self):
        marker=self.repo/'orphan'
        result=self.invoke('(sleep .6; echo orphan > "$R/orphan") &\necho done > "$O/output"\n')
        self.assertEqual(result.returncode,0,result.stderr)
        time.sleep(.8);self.assertFalse(marker.exists())

    def test_group_termination_refusal_retains_failed_receipt_without_claiming_reap(self):
        self.script.write_text(self.prefix+'echo complete > "$O/output"\n')
        children=[]
        real_popen=subprocess.Popen
        def launch(*args,**kwargs):
            child=real_popen(*args,**kwargs);children.append(child);return child
        try:
            with patch('fixture_session.subprocess.Popen',side_effect=launch), patch('fixture_session.os.killpg',side_effect=PermissionError('controlled refusal')):
                code=supervise(self.repo,self.script,[])
            self.assertEqual(code,2)
        finally:
            for child in children:
                if child.returncode is None:
                    os.killpg(child.pid,signal.SIGKILL);child.wait(timeout=5)
        _,terminal=self.terminal();self.assertEqual(terminal['status'],'failed')
        self.assertFalse(terminal['owned_process_group_reaped'])
        self.assertFalse(terminal['pruning_authorized'])

    def test_group_signal_precedes_direct_child_reap_and_poll_is_not_used(self):
        self.script.write_text(self.prefix+'echo complete > "$O/output"\n')
        real_popen = subprocess.Popen
        real_killpg = os.killpg
        selected = []
        def launch(*args, **kwargs):
            child = real_popen(*args, **kwargs)
            selected.append(child)
            child.poll = lambda: self.fail('Fixture must observe without reaping')
            return child
        def signal_group(pid, number):
            self.assertIsNone(selected[0].returncode)
            self.assertIsNone(os.waitid(os.P_PID, pid, os.WEXITED | os.WNOHANG | os.WNOWAIT))
            self.assertEqual(os.getpgid(pid),pid)
            return real_killpg(pid, number)
        with patch('fixture_session.subprocess.Popen', side_effect=launch), patch('fixture_session.os.killpg', side_effect=signal_group):
            self.assertEqual(supervise(self.repo, self.script, []), 0)
        _, terminal = self.terminal()
        self.assertTrue(terminal['direct_child_reaped'])
        self.assertFalse(terminal['owned_process_group_reaped'])

    def test_lost_or_changed_anchor_never_signals_saved_group_number(self):
        from fixture_session import signal_owned_group
        from types import SimpleNamespace
        child = SimpleNamespace(pid=123, returncode=0)
        with patch('fixture_session.os.killpg') as kill:
            with self.assertRaises(ValueError): signal_owned_group(child, signal.SIGKILL)
            kill.assert_not_called()
        child.returncode = None
        with patch('fixture_session.os.waitid', side_effect=ChildProcessError('lost anchor')), patch('fixture_session.os.killpg') as kill:
            with self.assertRaises(ChildProcessError): signal_owned_group(child, signal.SIGKILL)
            kill.assert_not_called()
        with patch('fixture_session.os.waitid', return_value=None), patch('fixture_session.os.getpgid', return_value=456), patch('fixture_session.os.killpg') as kill:
            with self.assertRaises(ValueError): signal_owned_group(child, signal.SIGKILL)
            kill.assert_not_called()

    def test_anchor_self_cleanup_on_parent_lifeline_loss(self):
        result_read,result_write=os.pipe()
        life_read,life_write=os.pipe()
        marker=self.repo/'parent-loss-child'
        self.script.write_text('#!/usr/bin/env bash\n(sleep .5; echo orphan > "'+str(marker)+'") &\nwait\n')
        process=subprocess.Popen([sys.executable,'-B',str(ROOT/'scripts/fixture_session.py'),'--anchor-child',str(result_write),str(life_read),'',str(self.script)],pass_fds=[result_write,life_read],start_new_session=True)
        os.close(result_write);os.close(life_read)
        try:
            time.sleep(.15)
            os.close(life_write);life_write=None
            self.assertEqual(process.wait(timeout=5),-signal.SIGKILL)
            time.sleep(.6);self.assertFalse(marker.exists())
        finally:
            if life_write is not None:os.close(life_write)
            os.close(result_read)
            if process.poll() is None:process.kill();process.wait()

    def test_result_pipe_rejects_oversized_malformed_and_out_of_range_results(self):
        self.script.write_text(self.prefix+'echo complete > "$O/output"\n')
        real_popen=subprocess.Popen
        for payload in (b'x'*17,b'not-an-exit\n',b'999\n'):
            def launch(command,**kwargs):
                result_fd=int(command[4])
                code='import os,signal;os.write('+str(result_fd)+','+repr(payload)+');signal.pause()'
                return real_popen([sys.executable,'-c',code],**kwargs)
            with patch('fixture_session.subprocess.Popen',side_effect=launch):
                self.assertEqual(supervise(self.repo,self.script,[]),2)
        receipts=[json.loads(path.read_text()) for path in (self.repo/'tmp/fixture-sessions').glob('*/receipt.json')]
        self.assertEqual(len(receipts),3)
        self.assertTrue(all(row['status']=='failed' and row['direct_child_reaped'] for row in receipts))

    def test_all_twenty_fixture_entrypoints_enter_supervision_before_allocation(self):
        matched=[]
        for script in (ROOT/'tests/integration').glob('run_*.sh'):
            text=script.read_text()
            if 'source ' in text and '/tests/integration/fixture_support.sh' in text:
                self.assertLess(text.index('physics_fixture_supervise'),text.index('$(physics_fixture_root'))
                matched.append(script)
        self.assertEqual(len(matched),20)


if __name__=='__main__':unittest.main()
