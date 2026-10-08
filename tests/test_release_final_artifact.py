"""Complete final artifact transaction with controlled fake authentication tools."""
import json
from pathlib import Path
import plistlib
import sys
import unittest
import shutil
from unittest.mock import patch
import test_release_app_stage as fixture

sys.path.insert(0,str(fixture.ROOT/'scripts'))
from release_app_stage import stage
from release_final_artifact import prepare, artifact_input, refresh
from desktop_replace import inventory

class FinalArtifact(unittest.TestCase):
    receipt=fixture.AppStage.receipt
    def setUp(self):
        fixture.AppStage.setUp(self)
        renamed=self.app.with_name('kinetiC.app');self.app.rename(renamed);self.app=renamed
        plist=self.app/'Contents/Info.plist'
        with plist.open('wb') as stream:
            plistlib.dump({'CFBundleIdentifier':'fixture.app','CFBundleName':'Fixture','CFBundleShortVersionString':'fixture-version'},stream)
        # Add architecture reporting before any signing tool identity is frozen.
        with self.tool.open('a') as stream:stream.write('if "-archs" in sys.argv:print("arm64")\n')
        signed=stage(self.repo,self.app,self.root,'fixture.app','sign',str(self.tool),'Developer ID: Fixture')
        self.sign_receipt=Path(signed['receipt']);self.signed_app=self.root/self.app.name
        self.notary_receipt=fixture.AppStage.acceptance(self)
        self.stapled_root=self.repo/'build/stapled'
        stapled=stage(self.repo,self.signed_app,self.stapled_root,'fixture.app','staple',str(self.tool),notary_receipt=self.notary_receipt)
        self.staple_receipt=Path(stapled['receipt']);self.stapled_app=self.stapled_root/self.app.name
        self.export_root=self.repo/'build/final export'
        self.identity={'product':'Fixture','program':'physics_sim','bundle_id':'fixture.app','version':'fixture-version',
                       'platform':'macos','arch':'arm64','channel':'fixture'}
        self.tools={key:str(self.tool) for key in ('codesign','xcrun','spctl','lipo','ditto')}

    def export(self,**kwargs):return prepare(self.repo,self.staple_receipt,self.export_root,self.identity,tools=self.tools,archive_name='final artifact.zip',**kwargs)
    def export_receipt(self):return next((self.export_root/'.package-transactions').glob('*/receipt.json'))

    def test_final_export_has_portable_sidecars_and_reuses_without_rewriting(self):
        before=inventory(self.stapled_app);result=self.export();self.assertEqual(result['status'],'completed')
        archive=self.export_root/'final artifact.zip';old=(archive.read_bytes(),archive.stat().st_mtime_ns)
        checksum=(self.export_root/'final artifact.zip.sha256').read_text();self.assertTrue(checksum.endswith('  final artifact.zip\n'))
        manifest=(self.export_root/'final artifact.manifest.txt').read_text()
        self.assertIn('artifact=final artifact.zip\n',manifest);self.assertIn('signed=1\nnotarized=1\n',manifest)
        self.assertIn('notary_json=final artifact.notary.json\n',manifest)
        notary=json.loads((self.export_root/'final artifact.notary.json').read_text())
        self.assertEqual(notary['status'],'Accepted');self.assertFalse(notary['public_or_registry_acceptance_verified'])
        self.assertEqual(self.export()['status'],'verified_reuse');self.assertEqual((archive.read_bytes(),archive.stat().st_mtime_ns),old)
        self.assertEqual(inventory(self.stapled_app),before)
        proof=json.loads((self.export_root/'command-reports/verification.json').read_text())
        self.assertTrue(proof['gatekeeper_command_succeeded']);self.assertEqual(proof['architecture'],'arm64')

    def test_gatekeeper_failure_preserves_stapled_app_and_publishes_nothing(self):
        tool=self.repo/'reject-gatekeeper';tool.write_text('#!/bin/sh\necho "internal error in Code Signing subsystem" >&2\nexit 7\n');tool.chmod(0o755)
        self.tools['spctl']=str(tool);before=inventory(self.stapled_app)
        with self.assertRaises(ValueError):self.export()
        row=json.loads(self.export_receipt().read_text());self.assertEqual(row['state'],'failed_retained');self.assertEqual(row['published'],[])
        self.assertFalse((self.export_root/'final artifact.zip').exists());self.assertEqual(inventory(self.stapled_app),before)
        self.assertIn('internal error',(self.export_receipt().parent/'stage/command-reports/gatekeeper.stderr').read_text())

    def test_architecture_or_bundle_version_mismatch_cannot_publish(self):
        self.identity['version']='wrong-version'
        with self.assertRaisesRegex(ValueError,'version/product'):self.export()
        self.assertFalse(self.export_root.exists());self.identity['version']='fixture-version'
        tool=self.repo/'wrong-architecture';tool.write_text('#!/bin/sh\necho x86_64\n');tool.chmod(0o755)
        self.tools['lipo']=str(tool)
        with self.assertRaises(ValueError):self.export()
        self.assertFalse((self.export_root/'final artifact.zip').exists())

    def test_partial_archive_failure_is_retained_without_final_outputs(self):
        tool=self.repo/'partial-final-archive';tool.write_text('#!'+sys.executable+'\nfrom pathlib import Path\nimport sys\nPath(sys.argv[-1]).write_bytes(b"partial");raise SystemExit(7)\n');tool.chmod(0o755)
        self.tools['ditto']=str(tool)
        with self.assertRaises(ValueError):self.export()
        receipt=self.export_receipt();row=json.loads(receipt.read_text());self.assertEqual(row['published'],[])
        self.assertEqual((receipt.parent/'stage/final artifact.zip').read_bytes(),b'partial')
        self.assertFalse((self.export_root/'final artifact.manifest.txt').exists())

    def test_unknown_predecessor_and_acceptance_drift_hold_without_overwrite(self):
        self.export_root.mkdir(parents=True);file=self.export_root/'final artifact.zip';file.write_bytes(b'unknown predecessor')
        with self.assertRaises(ValueError):self.export()
        self.assertEqual(file.read_bytes(),b'unknown predecessor')
        self.export_root=self.repo/'build/acceptance drift'
        state=json.loads(self.notary_receipt.read_text());proof=self.notary_receipt.parent/state['queries'][-1]['directory']/'notary.stdout'
        proof.write_text(json.dumps({'id':state['submission_id'],'status':'Invalid'}))
        with self.assertRaises(ValueError):self.export()
        self.assertFalse(self.export_root.exists())

    def test_refresh_uses_bound_stapled_app_and_preserves_installed_predecessor(self):
        result=self.export();receipt=Path(result['receipt'])
        home=self.repo/'home';destination=home/'Desktop/kinetiC.app';destination.parent.mkdir(parents=True)
        shutil.copytree(self.stapled_app,destination,symlinks=True)
        (destination/'predecessor.marker').write_bytes(b'old installed app')
        with patch('desktop_replace.Path.home',return_value=home):
            refreshed=refresh(self.repo,receipt,destination)
        self.assertEqual(inventory(destination),inventory(self.stapled_app))
        self.assertFalse((destination/'predecessor.marker').exists())
        attempts=list((home/'Desktop/.physics-sim-app-history').glob('*/receipt.json'))
        self.assertEqual(len(attempts),1)
        self.assertTrue((attempts[0].parent/'predecessor/kinetiC.app/predecessor.marker').exists())
        self.assertEqual(artifact_input(self.repo,receipt)[0],self.stapled_app)

    def test_final_sidecar_tampering_holds_refresh_before_desktop_allocation(self):
        result=self.export();receipt=Path(result['receipt'])
        (self.export_root/'final artifact.notary.json').write_text('{}')
        home=self.repo/'home';destination=home/'Desktop/kinetiC.app'
        with patch('desktop_replace.Path.home',return_value=home):
            with self.assertRaises(ValueError):refresh(self.repo,receipt,destination)
        self.assertFalse((home/'Desktop').exists())

if __name__=='__main__':unittest.main()
