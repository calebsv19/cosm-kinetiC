"""Supervision contract: immutable success/failure reuse and artifact identity."""
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import run_cfd_reference3d_method as runner


class ReceiptContract(unittest.TestCase):
    def fixture(self,root,exit_code):
        (root/'scripts').mkdir();(root/'build/cfd-reference-venv/bin').mkdir(parents=True)
        (root/'build/cfd-reference-venv/bin/python').symlink_to(sys.executable)
        (root/'scripts/fixture.py').write_text(
            "import argparse\nfrom pathlib import Path\n"
            "p=argparse.ArgumentParser();p.add_argument('--snapshot');p.add_argument('--output');a=p.parse_args()\n"
            "counter=Path('launches');counter.write_text(str(int(counter.read_text())+1) if counter.exists() else '1')\n"
            "print('fixture executed',flush=True)\n"
            + ("Path(a.output).write_text('{}');Path(a.snapshot).write_bytes(b'fixture')\n" if exit_code==0 else '')
            + f"raise SystemExit({exit_code})\n")

    def exercise(self,exit_code,tamper=False,changed_command=False):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.fixture(root,exit_code)
            with patch.multiple(runner,ROOT=root,DATA=root/'runs',SOURCES=('fixture.py',)), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(runner.run('test',[]),exit_code==0)
                receipt_path=next((root/'runs').glob('*/test-receipt.json'))
                before=receipt_path.read_bytes();receipt=json.loads(before)
                self.assertEqual(receipt['returncode'],exit_code)
                if tamper:
                    next((root/'runs').glob('*/test.log')).write_text('changed')
                    with self.assertRaises(AssertionError):runner.run('test',[])
                elif changed_command:
                    with self.assertRaises(AssertionError):runner.run('test',['--changed'])
                else:self.assertEqual(runner.run('test',[]),exit_code==0)
                self.assertEqual((root/'launches').read_text(),'1')
                self.assertEqual(receipt_path.read_bytes(),before)
                if exit_code:self.assertEqual(receipt['diagnostic_failure']['kind'],'child_error')

    def test_success_reuse(self):self.exercise(0)
    def test_failed_child_is_not_retried(self):self.exercise(7)
    def test_changed_artifact_is_rejected(self):self.exercise(0,tamper=True)
    def test_changed_command_is_rejected(self):self.exercise(0,changed_command=True)


if __name__=='__main__':unittest.main()
