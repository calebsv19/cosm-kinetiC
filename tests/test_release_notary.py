"""Exact notarization journal binding with fake tools; no Apple service calls."""
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import test_release_app_stage as fixture

sys.path.insert(0,str(fixture.ROOT/'scripts'))
from release_app_stage import stage
from package_transaction import run as transaction
from desktop_replace import digest, write_record
from contract_proof import IncompleteTeardown, execute
from release_notary import run
from release_notary_archive import prepare

SUBMISSION='12345678-1234-4234-8234-123456789abc'

class Notary(unittest.TestCase):
    def setUp(self):
        fixture.AppStage.setUp(self)
        signed_root=self.repo/'build/signed'
        signed=stage(self.repo,self.app,signed_root,'fixture.app','sign',str(self.tool),'Developer ID: Fixture')
        self.sign_receipt=Path(signed['receipt']);self.signed_app=signed_root/self.app.name
        archive_root=self.repo/'build/archive';self.archive=archive_root/'app.zip'
        result=prepare(self.repo,self.sign_receipt,archive_root,str(self.tool),'app.zip')
        self.archive_receipt=Path(result['receipt']);self.notary_root=self.repo/'build/notary'
        self.notary_tool=self.repo/'fake-xcrun';self.requests=self.repo/'requests.jsonl'
        self.behavior=self.repo/'behavior.json';self.behavior.write_text('{}')
        self.notary_tool.write_text('#!'+sys.executable+'\nimport sys,json\nfrom pathlib import Path\n'+
            'b=json.loads(Path('+repr(str(self.behavior))+').read_text())\n'+
            'with Path('+repr(str(self.requests))+').open("a") as f:f.write(json.dumps(sys.argv[1:])+"\\n")\n'+
            'if b.get("malformed"):print("not JSON")\n'+
            'else:print(json.dumps({"id":b.get("id",'+repr(SUBMISSION)+'),"status":b.get("status","Accepted")}))\n'+
            'raise SystemExit(b.get("exit",0))\n')
        self.notary_tool.chmod(0o755)

    def notarize(self,**kwargs):
        return run(self.repo,self.archive,self.notary_root,self.archive_receipt,'fixture profile',str(self.notary_tool),**kwargs)

    def request_list(self):return [json.loads(line) for line in self.requests.read_text().splitlines()]
    def behavior_set(self,**values):self.behavior.write_text(json.dumps(values))

    def test_submit_once_reconcile_accept_and_reuse_exact_proof(self):
        first=self.notarize();self.assertEqual(first['state'],'submitted')
        self.assertEqual(first['submission_id'],SUBMISSION)
        self.assertIn('--no-wait',self.request_list()[0]);self.assertNotIn('--wait',self.request_list()[0])
        self.assertIn('fixture profile',self.request_list()[0])
        with self.assertRaisesRegex(ValueError,'never resubmit'):self.notarize()
        accepted=self.notarize(reconcile=True);self.assertEqual(accepted['state'],'accepted')
        self.assertEqual(self.notarize()['state'],'accepted')
        self.assertEqual([item[1] for item in self.request_list()],['submit','info'])
        self.assertFalse(accepted['release_authority_granted'])

    def test_nonzero_submit_with_complete_response_can_reconcile_without_resubmit(self):
        self.behavior_set(exit=7)
        with self.assertRaises(ValueError):self.notarize()
        self.behavior_set()
        self.assertEqual(self.notarize(reconcile=True)['state'],'accepted')
        self.assertEqual([item[1] for item in self.request_list()],['submit','info'])

    def test_unknown_submission_is_held_without_second_submit(self):
        self.behavior_set(malformed=True)
        with self.assertRaises(ValueError):self.notarize()
        self.behavior_set()
        with self.assertRaises(ValueError):self.notarize(reconcile=True)
        with self.assertRaises(ValueError):self.notarize()
        self.assertEqual(len(self.request_list()),1)

    def test_wrong_id_unknown_status_and_rejection_do_not_accept(self):
        self.notarize();self.behavior_set(id='22345678-1234-4234-8234-123456789abc')
        with self.assertRaisesRegex(ValueError,'another submission'):self.notarize(reconcile=True)
        self.behavior_set(status='Maybe')
        with self.assertRaisesRegex(ValueError,'Unknown notarization status'):self.notarize(reconcile=True)
        self.behavior_set(status='Invalid');self.assertEqual(self.notarize(reconcile=True)['state'],'rejected')
        self.assertEqual(sum(item[1]=='submit' for item in self.request_list()),1)

    def test_in_progress_requeries_same_id(self):
        self.notarize();self.behavior_set(status='In Progress')
        self.assertEqual(self.notarize(reconcile=True)['state'],'submitted')
        self.behavior_set();self.assertEqual(self.notarize(reconcile=True)['state'],'accepted')
        self.assertEqual(sum(item[1]=='submit' for item in self.request_list()),1)

    def test_archive_signing_and_profile_drift_hold_without_command(self):
        self.notarize();before=len(self.request_list())
        self.archive.write_bytes(b'drift')
        with self.assertRaises(ValueError):self.notarize(reconcile=True)
        self.assertEqual(len(self.request_list()),before)

    def test_profile_or_signed_payload_drift_prevents_reconciliation(self):
        self.notarize()
        with self.assertRaisesRegex(ValueError,'identity changed'):
            run(self.repo,self.archive,self.notary_root,self.archive_receipt,'changed profile',str(self.notary_tool),reconcile=True)
        (self.signed_app/'Contents/MacOS/physics-sim-bin').write_bytes(b'changed signed payload')
        with self.assertRaises(ValueError):self.notarize(reconcile=True)
        self.assertEqual(len(self.request_list()),1)

    def test_durable_intent_precedes_submit_and_uppercase_uuid_is_normalized(self):
        self.behavior_set(id=SUBMISSION.upper())
        def observe(command,cwd,directory,tag,descriptors,wall_cap,log_cap):
            state=json.loads((self.notary_root/'receipt.json').read_text())
            self.assertEqual(state['state'],'submission_uncertain')
            self.assertFalse(state['terminal_processes_verified'])
            self.assertEqual(state['queries'][-1]['operation'],'submit')
            self.assertEqual(state['binding']['archive_sha256'],digest(self.archive))
            return execute(command,cwd,directory,tag,descriptors,wall_cap,log_cap)
        self.assertEqual(self.notarize(run_command=observe)['submission_id'],SUBMISSION)
        self.assertEqual(self.notarize(reconcile=True)['state'],'accepted')

    def test_retained_response_cannot_escape_journal_during_reconciliation(self):
        self.behavior_set(malformed=True)
        with self.assertRaises(ValueError):self.notarize()
        receipt=self.notary_root/'receipt.json';state=json.loads(receipt.read_text())
        state['submit_stdout']='../external.json';write_record(receipt,state)
        with self.assertRaisesRegex(ValueError,'response path'):self.notarize(reconcile=True)
        self.assertEqual(len(self.request_list()),1)

    def test_accepted_response_tampering_is_detected_on_reuse(self):
        self.notarize();state=self.notarize(reconcile=True)
        path=self.notary_root/state['queries'][-1]['directory']/'notary.stdout'
        path.write_text(json.dumps({'id':SUBMISSION,'status':'Invalid'}))
        with self.assertRaisesRegex(ValueError,'proof changed'):self.notarize()
        self.assertEqual(len(self.request_list()),2)

    def test_unverified_submit_teardown_blocks_reconciliation(self):
        def uncertain(*args,**kwargs):raise IncompleteTeardown('unknown termination')
        with self.assertRaises(IncompleteTeardown):self.notarize(run_command=uncertain)
        with self.assertRaisesRegex(ValueError,'teardown unverified'):self.notarize(reconcile=True)
        self.assertFalse(self.requests.exists())

    def test_protected_and_unknown_journals_hold_before_any_submission(self):
        self.notary_root=self.repo/'src'
        with self.assertRaises(ValueError):self.notarize()
        self.assertFalse(self.notary_root.exists())
        self.notary_root=self.repo/'build/notary';self.notary_root.mkdir(parents=True)
        (self.notary_root/'sentinel').write_bytes(b'preserved')
        with self.assertRaises(ValueError):self.notarize()
        self.assertEqual((self.notary_root/'sentinel').read_bytes(),b'preserved')
        self.assertFalse(self.requests.exists())

if __name__=='__main__':unittest.main()
