"""Real archive producer and acceptance-to-staple gates, with fake SDK tools."""
import json
from pathlib import Path
import sys
import unittest
import test_release_app_stage as fixture

sys.path.insert(0,str(fixture.ROOT/'scripts'))
from release_app_stage import stage
from release_notary_archive import prepare
from release_notary import archive_binding, accepted_binding
from desktop_replace import inventory

class Archive(unittest.TestCase):
    def setUp(self):
        fixture.AppStage.setUp(self)
        self.signed=stage(self.repo,self.app,self.root,'fixture.app','sign',str(self.tool),'Developer ID: Fixture')
        self.sign_receipt=Path(self.signed['receipt']);self.signed_app=self.root/self.app.name
        self.archive_root=self.repo/'build/notary archive'

    def prepare(self,**kwargs):return prepare(self.repo,self.sign_receipt,self.archive_root,str(self.tool),**kwargs)

    def test_archive_preserves_signed_input_and_reuses_portable_sidecars(self):
        before=inventory(self.signed_app)
        result=self.prepare(archive_name='upload with spaces.zip');self.assertEqual(result['status'],'completed')
        archive=self.archive_root/'upload with spaces.zip'
        binding=archive_binding(self.repo,archive,Path(result['receipt']))
        self.assertEqual((self.archive_root/'upload with spaces.zip.sha256').read_text(),binding['archive_sha256']+'  upload with spaces.zip\n')
        manifest=json.loads((self.archive_root/'upload with spaces.zip.manifest.json').read_text())
        self.assertEqual(manifest['artifact'],archive.name);self.assertEqual(manifest['sha256'],binding['archive_sha256'])
        self.assertEqual(inventory(self.signed_app),before)
        old=(archive.read_bytes(),archive.stat().st_mtime_ns)
        self.assertEqual(self.prepare(archive_name=archive.name)['status'],'verified_reuse')
        self.assertEqual((archive.read_bytes(),archive.stat().st_mtime_ns),old)

    def test_archive_failure_keeps_partial_stage_and_publishes_nothing(self):
        # A distinct archive tool avoids changing the already-bound signing tool.
        tool=self.repo/'broken-archive';tool.write_text('#!'+sys.executable+'\nfrom pathlib import Path\nimport sys\nPath(sys.argv[-1]).write_bytes(b"partial");raise SystemExit(7)\n');tool.chmod(0o755)
        before=inventory(self.signed_app)
        with self.assertRaises(ValueError):prepare(self.repo,self.sign_receipt,self.archive_root,str(tool))
        receipt=next((self.archive_root/'.package-transactions').glob('*/receipt.json'))
        row=json.loads(receipt.read_text());self.assertEqual(row['state'],'failed_retained');self.assertEqual(row['published'],[])
        self.assertEqual((receipt.parent/'stage/notary-upload.zip').read_bytes(),b'partial')
        self.assertFalse((self.archive_root/'notary-upload.zip').exists());self.assertEqual(inventory(self.signed_app),before)

    def test_unsigned_stage_and_protected_root_hold_before_archive_creation(self):
        other=stage(self.repo,self.app,self.repo/'build/adhoc','fixture.app','sign',str(self.tool))
        with self.assertRaisesRegex(ValueError,'Developer ID'):prepare(self.repo,Path(other['receipt']),self.archive_root,str(self.tool))
        self.assertFalse(self.archive_root.exists())
        with self.assertRaises(ValueError):prepare(self.repo,self.sign_receipt,self.repo/'src',str(self.tool))
        self.assertFalse((self.repo/'src').exists())

    def test_staple_requires_acceptance_and_the_exact_signed_app(self):
        staple_root=self.repo/'build/staple held'
        with self.assertRaisesRegex(ValueError,'notarization receipt'):
            stage(self.repo,self.signed_app,staple_root,'fixture.app','staple',str(self.tool))
        self.assertFalse(staple_root.exists())
        acceptance=fixture.AppStage.acceptance(self)
        with self.assertRaisesRegex(ValueError,'this signed app'):
            accepted_binding(self.repo,acceptance,self.app)
        before=inventory(self.signed_app)
        result=stage(self.repo,self.signed_app,staple_root,'fixture.app','staple',str(self.tool),notary_receipt=acceptance)
        self.assertEqual(result['status'],'completed');self.assertEqual(inventory(self.signed_app),before)
        row=json.loads((staple_root/'command-reports/stage.json').read_text())
        self.assertEqual(row['notary_acceptance']['binding']['signed_app'],str(self.signed_app))

    receipt=fixture.AppStage.receipt

    def test_acceptance_tampering_holds_before_staple_tool_or_allocation(self):
        acceptance=fixture.AppStage.acceptance(self)
        state=json.loads(acceptance.read_text());proof=acceptance.parent/state['queries'][-1]['directory']/'notary.stdout'
        proof.write_text(json.dumps({'id':state['submission_id'],'status':'Invalid'}))
        target=self.repo/'build/staple tampered'
        with self.assertRaisesRegex(ValueError,'proof changed'):
            stage(self.repo,self.signed_app,target,'fixture.app','staple',str(self.tool),notary_receipt=acceptance)
        self.assertFalse(target.exists())

    def test_unknown_archive_predecessor_and_filename_escape_are_held(self):
        self.archive_root.mkdir(parents=True);file=self.archive_root/'notary-upload.zip';file.write_bytes(b'predecessor')
        with self.assertRaises(ValueError):self.prepare()
        self.assertEqual(file.read_bytes(),b'predecessor')
        for name in ('../outside.zip','line\nbreak.zip','bad.tar'):
            with self.assertRaises(ValueError):self.prepare(archive_name=name)

if __name__=='__main__':unittest.main()
