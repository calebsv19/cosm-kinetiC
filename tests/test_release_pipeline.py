"""End-to-end release command cutover using fixture authentication tools only."""
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import unittest
from unittest.mock import patch
import test_release_app_stage as fixture

sys.path.insert(0, str(fixture.ROOT / 'scripts'))
from release_pipeline import run
from desktop_replace import inventory
from release_final_artifact import artifact_input


class Pipeline(unittest.TestCase):
    def setUp(self):
        fixture.AppStage.setUp(self)
        app = self.app.with_name('kinetiC.app'); self.app.rename(app); self.app = app
        with (app / 'Contents/Info.plist').open('wb') as stream:
            plistlib.dump({'CFBundleIdentifier':'fixture.app','CFBundleName':'Fixture',
                          'CFBundleShortVersionString':'fixture-version'},stream)
        text = self.tool.read_text().replace('"status":"Accepted"',
            '"status":__import__("os").environ.get("FIXTURE_NOTARY_STATUS","Accepted")')
        self.tool.write_text(text + '\nif "-archs" in sys.argv:print("arm64")\n')
        self.before = inventory(self.app)
        self.output = self.repo / 'build/release/authenticated'
        self.identity = {'product':'Fixture','program':'physics_sim','bundle_id':'fixture.app',
            'version':'fixture-version','platform':'macos','arch':'arm64','channel':'fixture'}
        self.tools = {key:str(self.tool) for key in ('codesign','xcrun','spctl','lipo','ditto')}

    def invoke(self, action='artifact', signing='Developer ID: Fixture', profile='fixture-profile'):
        return run(self.repo,self.app,self.output,action,self.identity,signing,profile,
                   self.tools,'fixture.zip')

    def commands(self):
        return [json.loads(line) for line in self.journal.read_text().splitlines()]

    def test_complete_pipeline_preserves_source_and_reuses_all_phases(self):
        result=self.invoke(); receipt=Path(result['receipt'])
        self.assertEqual(artifact_input(self.repo,receipt)[0],self.output/'stapled/kinetiC.app')
        before=self.commands();self.assertEqual(self.invoke()['status'],'verified_reuse')
        self.assertEqual(self.commands(),before);self.assertEqual(inventory(self.app),self.before)
        self.assertEqual(sum('submit' in cmd for cmd in before),1)
        self.assertEqual(sum('info' in cmd for cmd in before),1)
        self.assertFalse((self.output.parent/'fixture.zip').exists())

    def test_pending_stops_downstream_then_queries_same_id_without_resubmit(self):
        with patch.dict(os.environ,{'FIXTURE_NOTARY_STATUS':'In Progress'}):
            with self.assertRaisesRegex(ValueError,'submitted'):self.invoke()
        self.assertFalse((self.output/'stapled').exists());self.assertFalse((self.output/'final').exists())
        result=self.invoke();self.assertEqual(result['status'],'completed')
        cmds=self.commands();self.assertEqual(sum('submit' in cmd for cmd in cmds),1)
        infos=[cmd for cmd in cmds if 'info' in cmd];self.assertEqual(len(infos),2)
        self.assertEqual(infos[0][2],infos[1][2])

    def test_rejected_notary_and_missing_identity_never_reach_staple(self):
        with self.assertRaises(ValueError):self.invoke(signing='-')
        self.assertFalse(self.output.exists())
        with patch.dict(os.environ,{'FIXTURE_NOTARY_STATUS':'Invalid'}):
            with self.assertRaisesRegex(ValueError,'rejected'):self.invoke()
        self.assertFalse((self.output/'stapled').exists())
        self.assertEqual(inventory(self.app),self.before)

    def test_metadata_drift_holds_before_any_authentication_or_output(self):
        self.identity['version']='wrong-version'
        with self.assertRaisesRegex(ValueError,'version/product'):self.invoke()
        self.assertFalse(self.output.exists());self.assertFalse(self.journal.exists())
        self.assertEqual(inventory(self.app),self.before)

    def test_unknown_phase_output_and_input_overlap_hold(self):
        self.output=self.app/'nested'
        with self.assertRaisesRegex(ValueError,'overlaps'):self.invoke()
        self.assertEqual(inventory(self.app),self.before)
        self.output=self.repo/'build/release/authenticated'
        path=self.output/'signed/kinetiC.app';path.mkdir(parents=True);(path/'unknown').write_bytes(b'held')
        with self.assertRaises(ValueError):self.invoke()
        self.assertEqual((path/'unknown').read_bytes(),b'held')

    def test_make_entrypoint_runs_bound_pipeline_and_refresh_requires_receipt(self):
        shutil.copytree(fixture.ROOT/'scripts',self.repo/'scripts')
        makefile=self.repo/'fixture.mk'
        makefile.write_text('include '+str(fixture.ROOT/'make/release.mk')+'\n'
            '.PHONY: release-bundle-audit package-desktop-self-test package-desktop-refresh-authority\n'
            'release-bundle-audit:\n\t@true\n'
            'package-desktop-self-test package-desktop-refresh-authority:\n\t@true\n')
        options=['PACKAGE_APP_DIR='+str(self.app),'RELEASE_DIR='+str(self.output.parent),
            'RELEASE_PRODUCT_NAME=Fixture','RELEASE_PROGRAM_KEY=physics_sim','RELEASE_BUNDLE_ID=fixture.app',
            'RELEASE_VERSION=fixture-version','RELEASE_CHANNEL=fixture','RELEASE_PLATFORM=macOS',
            'RELEASE_ARCH=arm64','RELEASE_ARTIFACT_BASENAME=fixture','RELEASE_CODESIGN_IDENTITY=Developer ID: Fixture',
            'APPLE_NOTARY_PROFILE=fixture-profile']
        options += ['RELEASE_'+key.upper()+'='+str(self.tool) for key in self.tools]
        cmd=['make','-f',str(makefile)]+options
        result=subprocess.run(cmd+['release-distribute'],cwd=self.repo,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        receipt=next((self.output/'final/.package-transactions').glob('*/receipt.json'))
        self.assertEqual(json.loads(receipt.read_text())['state'],'completed')
        before=self.commands()
        result=subprocess.run(cmd+['release-distribute'],cwd=self.repo,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertEqual(self.commands(),before)
        result=subprocess.run(cmd+['release-desktop-refresh'],cwd=self.repo,capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0);self.assertIn('RELEASE_FINAL_RECEIPT is required',result.stdout)
        self.assertEqual(inventory(self.app),self.before)


if __name__=='__main__':unittest.main()
