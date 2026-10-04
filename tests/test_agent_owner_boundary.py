"""Deterministic publication/owner-exit race; no timing-sensitive solver loop."""
import fcntl
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service, atomic, read

class OwnerBoundaryTests(unittest.TestCase):
    def setup_run(self, root):
        service=Service(root)
        path=Path(root)/'runs'/'run';path.mkdir()
        atomic(path/'request.json',{'run_id':'run','scene_revision':'revision'})
        atomic(path/'snapshot.json',{'state':'running','tick':24,'sequence':27,'simulation_time':.48})
        (path/'events.jsonl').write_text('')
        return service,path

    def test_exit_publication_wins_over_earlier_running_read(self):
        # The owner finishes after _status reads 'running' but before its lock
        # check. The durable terminal publication must be re-read, never replaced.
        for terminal in ('completed','cancelled','failed'):
            with self.subTest(terminal=terminal), tempfile.TemporaryDirectory() as root:
                service,path=self.setup_run(root)
                expected={'state':terminal,'tick':25,'sequence':28,'simulation_time':.5,
                          'error':'numerical_failure' if terminal=='failed' else ''}
                original=fcntl.flock
                def transition(fd, operation):
                    if operation==fcntl.LOCK_EX|fcntl.LOCK_NB:
                        atomic(path/'snapshot.json',expected)
                        (path/'events.jsonl').write_text(json.dumps({'cursor':28,'tick':25,'state':terminal})+'\n')
                    return original(fd,operation)
                with patch('service.fcntl.flock',side_effect=transition):
                    result=service._status(path)
                self.assertEqual(result,expected)
                self.assertEqual(read(path/'snapshot.json'),expected)
                self.assertEqual(len((path/'events.jsonl').read_text().splitlines()),1)
                self.assertEqual(service._status(path),expected)

    def test_actual_owner_death_is_reconciled_once(self):
        with tempfile.TemporaryDirectory() as root:
            service,path=self.setup_run(root)
            result=service._status(path)
            self.assertEqual(result['state'],'failed')
            self.assertEqual(result['error'],'worker_exited_without_terminal_result')
            self.assertEqual(result['tick'],24)
            self.assertEqual(result['sequence'],28)
            self.assertEqual(service._status(path),result)
            self.assertEqual(len((path/'events.jsonl').read_text().splitlines()),1)

if __name__=='__main__':unittest.main()
