"""Local-session diagnostic history preserves bytes and request identities."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
import sample_retention as history
from service import Service, SessionError


class Samples(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.run=Path(self.temp.name).resolve()/'runs/run';self.run.mkdir(parents=True)
        for name in ('sample_requests','sample_results','sample_ids'):(self.run/name).mkdir()
        self.payload={'request_id':'old','field':'speed'}
        self.request=self.run/'sample_ids/old.json';self.request.write_text(json.dumps(self.payload))
        self.result=self.run/'sample_results/old.json';self.result.write_text('{"status":"ready"}')

    def test_copy_readback_then_retire_preserves_exact_bytes(self):
        before=(self.request.read_bytes(),self.result.read_bytes())
        selected=history.retire(self.run,'old')
        self.assertEqual((selected/'request.json').read_bytes(),before[0]);self.assertEqual((selected/'result.json').read_bytes(),before[1])
        self.assertFalse(self.request.exists());self.assertFalse(self.result.exists())
        self.assertEqual(history.verify(self.run,'old'),selected)

    def test_history_flush_failure_preserves_active_copies(self):
        with patch.object(history,'flush_directory',side_effect=OSError('fixture flush failure')):
            with self.assertRaises(OSError):history.retire(self.run,'old')
        self.assertTrue(self.request.exists());self.assertTrue(self.result.exists())
        history.verify(self.run,'old')
        history.retire(self.run,'old');self.assertFalse(self.request.exists())

    def test_pending_age_never_permits_retirement(self):
        pending=self.run/'sample_requests/old.json';pending.write_bytes(self.request.read_bytes());os.utime(pending,(1,1))
        with self.assertRaisesRegex(ValueError,'Pending'):history.retire(self.run,'old')
        self.assertTrue(self.request.exists());self.assertTrue(self.result.exists());self.assertTrue(pending.exists())
        self.assertFalse((self.run/'sample_history').exists())

    def test_partial_copy_failure_preserves_active_inputs_and_exact_retry(self):
        original=history.retain
        def fail_receipt(path,data):
            if path.name=='receipt.json':raise OSError('fixture interruption')
            return original(path,data)
        with patch.object(history,'retain',side_effect=fail_receipt):
            with self.assertRaises(OSError):history.retire(self.run,'old')
        self.assertTrue(self.request.exists());self.assertTrue(self.result.exists())
        self.assertEqual(history.verify(self.run,'old') if (self.run/'sample_history/old/receipt.json').exists() else None,None)
        history.retire(self.run,'old');history.verify(self.run,'old')

    def test_resume_after_partial_active_removal_uses_verified_history(self):
        original=Path.unlink
        def fail_request(path,*args,**kwargs):
            if path==self.request:raise OSError('fixture interruption')
            return original(path,*args,**kwargs)
        # Unlink result first to reproduce an interrupted two-file retirement.
        with patch.object(Path,'unlink',side_effect=fail_request,autospec=True):
            with self.assertRaises(OSError):history.retire(self.run,'old')
        self.result.unlink();self.assertTrue(self.request.exists())
        history.retire(self.run,'old');self.assertFalse(self.request.exists());history.verify(self.run,'old')

    def test_unknown_changed_or_symlink_history_holds_active_inputs(self):
        selected=history.location(self.run,'old');selected.mkdir(parents=True)
        (selected/'request.json').write_bytes(b'unknown')
        with self.assertRaises(ValueError):history.retire(self.run,'old')
        self.assertTrue(self.request.exists());self.assertTrue(self.result.exists())
        (selected/'request.json').unlink();(selected/'request.json').symlink_to(self.request)
        with self.assertRaises(ValueError):history.retire(self.run,'old')
        self.assertTrue(self.request.exists());self.assertTrue(self.result.exists())

    def test_oversize_and_request_limit_are_held(self):
        with patch.dict(history.LIMITS,{'result.json':2}):
            with self.assertRaises(ValueError):history.retire(self.run,'old')
        with patch.object(history,'MAX_REQUESTS',1):
            with self.assertRaisesRegex(ValueError,'limit'):history.admit_new(self.run)
        self.assertTrue(self.request.exists());self.assertTrue(self.result.exists())

    def test_history_enumeration_is_bounded_and_refuses_unknown_types(self):
        selected=history.retire(self.run,'old')
        self.assertEqual(history.history_entries(self.run),['old'])
        (selected.parent/'extra').mkdir()
        with patch.object(history,'MAX_REQUESTS',1):
            with self.assertRaisesRegex(ValueError,'bound/type'):history.history_entries(self.run)
        (selected.parent/'extra').rmdir();(selected.parent/'unknown').write_bytes(b'held')
        with self.assertRaisesRegex(ValueError,'bound/type'):history.history_entries(self.run)

    def test_service_retry_of_retired_id_reads_history_and_rejects_changed_payload(self):
        service=Service(self.run.parents[1]);(self.run/'request.json').write_text('{"model":"wind_approximate_v1"}')
        payload={'request_id':'old','plane':'XY','position':.5,'resolution':48,'field':'speed','points':[],
                 'color_range':None,'vectors':False}
        self.request.write_text(json.dumps(payload));self.result.write_text(json.dumps({'status':'ready','preview':{},'sampled_at':1}))
        history.retire(self.run,'old')
        result=service.run_sample('run','old');self.assertEqual(result['status'],'ready')
        self.assertFalse(self.request.exists())
        with self.assertRaisesRegex(SessionError,'conflict'):service.run_sample('run','old',resolution=4)

    def test_service_full_pending_queue_preserves_old_requests(self):
        service=Service(self.run.parents[1]);(self.run/'request.json').write_text('{"model":"wind_approximate_v1"}')
        for i in range(8):
            path=self.run/'sample_requests'/f'p{i}.json';path.write_text('{}');os.utime(path,(1,1))
        with patch.object(service,'_status',return_value={'state':'paused','sample_protocol':1}):
            with self.assertRaisesRegex(SessionError,'queue_full'):service.run_sample('run','new')
        self.assertEqual(len(list((self.run/'sample_requests').iterdir())),8)
        self.assertFalse((self.run/'sample_ids/new.json').exists())

if __name__=='__main__':unittest.main()
