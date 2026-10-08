"""Retained worker diagnostics include execution and failed acceptance."""
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from atmosphere_attempt import retained_atmosphere, retained_directory, CONTROLS
from passive_atmosphere import run_atmosphere_worker
import passive_atmosphere,evolving_atmosphere,open_atmosphere

class Attempts(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name)
        for name in CONTROLS:
            dest=self.repo/'scripts'/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/'scripts'/name,dest)
        self.worker=self.repo/'worker';self.worker.write_text('#!/bin/sh\nprintf {}\nprintf diagnostic >&2\n');self.worker.chmod(0o700)
    def capsules(self):return sorted((self.repo/'data/experiments/atmosphere-attempts').iterdir())
    def operation(self,accept=True):
        @retained_atmosphere(self.repo,'test')
        def run(request,worker):
            with retained_directory() as directory:
                p=directory/'request.json';p.write_text(json.dumps(request))
                output=run_atmosphere_worker(worker,p)
                (directory/'fields.json').write_bytes(output)
            if not accept:raise ValueError('acceptance failed')
            return {'status':'accepted'}
        return run
    def test_completed_and_failed_reruns_are_distinct_and_preserved(self):
        self.operation()({'input':1},self.worker)
        with self.assertRaisesRegex(ValueError,'retained attempt'):self.operation(False)({'input':2},self.worker)
        capsules=self.capsules();self.assertEqual(len(capsules),2)
        states={json.loads((p/'receipt.json').read_text())['status']:p for p in capsules}
        self.assertEqual(set(states),{'completed','failed_retained'})
        for p in capsules:
            self.assertEqual((p/'stdout.bin').read_bytes(),b'{}');self.assertEqual((p/'stderr.bin').read_bytes(),b'diagnostic')
            self.assertTrue((p/'request.json').exists());self.assertTrue((p/'source/worker').exists())
        self.assertFalse((states['failed_retained']/'accepted.json').exists())
    def test_nonzero_worker_preserves_partial_logs(self):
        self.worker.write_text('#!/bin/sh\nprintf partial\nprintf why >&2\nexit 7\n')
        with self.assertRaises(ValueError):self.operation()({},self.worker)
        p=self.capsules()[0];state=json.loads((p/'receipt.json').read_text())
        self.assertEqual(state['status'],'failed_retained');self.assertEqual(state['command']['exit_code'],7)
        self.assertEqual((p/'stdout.bin').read_bytes(),b'partial');self.assertEqual((p/'stderr.bin').read_bytes(),b'why')
    def test_unverified_teardown_is_held(self):
        with patch('passive_atmosphere.capture',return_value={'status':'unverified','reason':'teardown','terminal_processes_verified':False,'stdout':b'partial','stderr':b'why'}):
            with self.assertRaises(ValueError):self.operation()({},self.worker)
        state=json.loads((self.capsules()[0]/'receipt.json').read_text())
        self.assertEqual(state['status'],'held');self.assertFalse(state['terminal_processes_verified'])
    def test_changed_worker_holds_acceptance(self):
        @retained_atmosphere(self.repo,'test')
        def run(request,worker):
            with retained_directory() as p:
                request_path=p/'request.json';request_path.write_text('{}');run_atmosphere_worker(worker,request_path)
            worker.write_text('#!/bin/sh\nexit 0\n');return {}
        with self.assertRaisesRegex(ValueError,'input changed'):run({},self.worker)
        self.assertFalse((self.capsules()[0]/'accepted.json').exists())
    def test_preserved_control_corruption_refuses_completed_receipt(self):
        @retained_atmosphere(self.repo,'test')
        def run(request,worker):
            with retained_directory() as p:
                request_path=p/'request.json';request_path.write_text('{}');run_atmosphere_worker(worker,request_path)
                (p/'source/control/tool_probe.py').write_bytes(b'changed')
            return {}
        with self.assertRaisesRegex(ValueError,'input changed'):run({},self.worker)
        self.assertFalse((self.capsules()[0]/'accepted.json').exists())
    def test_missing_worker_retains_input(self):
        with self.assertRaises(OSError):self.operation()({'original':1},self.repo/'missing')
        p=self.capsules()[0];self.assertEqual(json.loads((p/'input.json').read_text()),{'original':1})
        self.assertEqual(json.loads((p/'receipt.json').read_text())['status'],'failed_retained')
    def test_linked_experiment_root_refused(self):
        outside=self.repo/'outside';outside.mkdir();link=self.repo/'linked';link.symlink_to(outside)
        with patch.dict(os.environ,{'PHYSICS_SIM_EXPERIMENT_ROOT':str(link)}):
            with self.assertRaises(ValueError):self.operation()({},self.worker)
        self.assertEqual(list(outside.iterdir()),[])
    def test_all_adapters_retain_request_on_malformed_fields(self):
        parent=self.repo/'adapter-evidence'
        with patch.dict(os.environ,{'PHYSICS_SIM_EXPERIMENT_ROOT':str(parent)}):
            for module in (passive_atmosphere,evolving_atmosphere,open_atmosphere):
                # Exclusion is independently covered by worker-owner tests.
                with patch.object(module,'validate',return_value=None):
                    with self.assertRaises((ValueError,KeyError)):
                        module.run.__wrapped__({'state':None,'grid':[4,4,4]},self.worker)
        capsules=list((parent/'atmosphere-attempts').iterdir());self.assertEqual(len(capsules),3)
        for p in capsules:
            self.assertTrue((p/'request.json').exists());self.assertEqual((p/'stdout.bin').read_bytes(),b'{}')
            self.assertTrue((p/'fields.json').exists());self.assertEqual(json.loads((p/'receipt.json').read_text())['status'],'failed_retained')

if __name__=='__main__':unittest.main()
