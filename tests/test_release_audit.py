"""Actual release audit recipes retain reruns and refuse unsafe report roots."""
import json
import os
import plistlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
ENV={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES') and not k.startswith('PHYSICS_SIM_BUILD_')}

class Audit(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();(self.repo/'scripts').mkdir();(self.repo/'make').mkdir()
        for name in ('package_proof.py','package_transaction.py','package_outputs.py','package_paths.py','build_owner.py','build_outputs.py','clean_outputs.py','check_clean_root.py','desktop_replace.py','contract_proof.py','cfd_evidence.py','release_framework_audit.py'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        (self.repo/'scripts/agent_session').mkdir()
        shutil.copy2(ROOT/'scripts/agent_session/owned_command.py',self.repo/'scripts/agent_session/owned_command.py')
        (self.repo/'tools/packaging').mkdir(parents=True)
        shutil.copy2(ROOT/'make/release.mk',self.repo/'make/release.mk')
        self.app=self.repo/'dist/Fake App.app';macos=self.app/'Contents/MacOS';macos.mkdir(parents=True)
        launcher=macos/'physics-sim-launcher';launcher.write_text('#!/bin/sh\nprintf "PHYSICS_SIM_RUNTIME_DIR=/tmp/fake-runtime\\nVK_ICD_FILENAMES=fake\\nVK_DRIVER_FILES=fake\\n"\n');launcher.chmod(0o755)
        (macos/'physics-sim-bin').write_bytes(b'fixture only')
        self.frameworks=self.app/'Contents/Frameworks'
        for folder in ('first folder','second folder'):
            path=self.frameworks/folder/'same name.dylib';path.parent.mkdir(parents=True);path.write_bytes(b'fixture dylib')
        self.plist=self.repo/'plist';self.plist.write_text('#!/bin/sh\necho com.fixture.app\n');self.plist.chmod(0o755)
        self.tool=self.repo/'otool';self.tool.write_text('#!/bin/sh\nprintf "%s:\\n\\t/usr/lib/libSystem.B.dylib\\n" "$2"\n');self.tool.chmod(0o755)
        (self.repo/'makefile').write_text('RELEASE_DIR=build/release\nRELEASE_BUNDLE_ID=com.fixture.app\nPACKAGE_APP_DIR='+str(self.app)+'\nPACKAGE_CONTENTS_DIR=$(PACKAGE_APP_DIR)/Contents\nPACKAGE_MACOS_DIR=$(PACKAGE_CONTENTS_DIR)/MacOS\nPACKAGE_FRAMEWORKS_DIR=$(PACKAGE_CONTENTS_DIR)/Frameworks\nRELEASE_PLIST_BUDDY='+str(self.plist)+'\nRELEASE_OTOOL='+str(self.tool)+'\npackage-desktop-self-test:\n\t@echo prerequisite >> prerequisite.log\ninclude make/release.mk\n')

    def make(self,*args):
        return subprocess.run(['make','-j2',*args],cwd=self.repo,env=ENV,text=True,capture_output=True,timeout=30)

    def test_two_actual_audits_retain_fixed_legacy_reports_and_each_framework(self):
        release=self.repo/'build/release';release.mkdir(parents=True);legacy=release/'print_config.txt';legacy.write_bytes(b'old diagnostic')
        for _ in range(2):
            result=self.make('release-bundle-audit');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertNotIn('jobserver unavailable',result.stderr)
        self.assertEqual(legacy.read_bytes(),b'old diagnostic')
        self.assertEqual(len((self.repo/'prerequisite.log').read_text().splitlines()),2)
        receipts=list((release/'.package-proofs').glob('*/receipt.json'));self.assertEqual(len(receipts),2)
        for receipt in receipts:
            row=json.loads(receipt.read_text());self.assertEqual(row['state'],'passed');self.assertTrue(row['terminal_processes_verified'])
            mapping=json.loads((receipt.parent/'work/framework-reports/index.json').read_text());self.assertEqual(len(mapping),2)
            self.assertNotEqual(mapping[0]['report'],mapping[1]['report'])
            for entry in mapping:self.assertIn(entry['framework'],(receipt.parent/'work/framework-reports'/entry['report']).read_text())

    def test_nonportable_framework_fails_with_retained_diagnostics(self):
        self.tool.write_text('#!/bin/sh\necho /opt/homebrew/lib/bad.dylib\n');self.tool.chmod(0o755)
        result=self.make('release-bundle-audit');self.assertNotEqual(result.returncode,0)
        receipt=next((self.repo/'build/release/.package-proofs').glob('*/receipt.json'));row=json.loads(receipt.read_text())
        self.assertEqual(row['state'],'failed_retained');self.assertTrue((receipt.parent/'work/otool_physics_sim_bin.txt').is_file())

    def test_framework_nonportable_dependency_retains_partial_report(self):
        self.tool.write_text('#!/bin/sh\ncase "$2" in *.dylib) echo /opt/homebrew/lib/bad.dylib;; *) echo /usr/lib/libSystem.B.dylib;; esac\n');self.tool.chmod(0o755)
        result=self.make('release-bundle-audit');self.assertNotEqual(result.returncode,0)
        receipt=next((self.repo/'build/release/.package-proofs').glob('*/receipt.json'))
        self.assertEqual(json.loads(receipt.read_text())['state'],'failed_retained')
        reports=receipt.parent/'work/framework-reports';self.assertIn('bad.dylib',(reports/'otool_1.txt').read_text());self.assertFalse((reports/'index.json').exists())

    def test_direct_framework_helper_protected_or_existing_outputs_hold(self):
        for directory in (self.repo/'src',self.repo/'build/prior-frameworks'):
            directory.mkdir(parents=True);sentinel=directory/'index.json';sentinel.write_bytes(b'preserved')
            result=subprocess.run([sys.executable,'-B','scripts/release_framework_audit.py','--frameworks',str(self.frameworks),'--reports',str(directory),'--otool',str(self.tool)],cwd=self.repo,env=ENV,text=True,capture_output=True,timeout=10)
            self.assertNotEqual(result.returncode,0);self.assertEqual(sentinel.read_bytes(),b'preserved');self.assertFalse((directory/'otool_1.txt').exists())

    def test_direct_audit_protected_or_reused_report_root_holds_before_writes(self):
        source=self.repo/'src';source.mkdir();sentinel=source/'bundle_id.txt';sentinel.write_bytes(b'protected')
        result=self.make('_release-bundle-audit','RELEASE_AUDIT_DIR='+str(source));self.assertNotEqual(result.returncode,0)
        self.assertEqual(sentinel.read_bytes(),b'protected');self.assertFalse((source/'framework-reports').exists())
        work=self.repo/'build/prior';work.mkdir(parents=True);(work/'print_config.txt').write_bytes(b'prior')
        result=self.make('_release-bundle-audit','RELEASE_AUDIT_DIR='+str(work));self.assertNotEqual(result.returncode,0)
        self.assertEqual((work/'print_config.txt').read_bytes(),b'prior');self.assertFalse((work/'framework-reports').exists())

    def test_only_exact_otool_file_headers_are_excluded_from_portability_check(self):
        binary=Path('/Users/fixture/CodeWork/header.bin')
        report=self.repo/'report.txt'
        command=[sys.executable,'-B','scripts/release_framework_audit.py','--verify-report',str(report),'--input-binary',str(binary)]
        for header in (str(binary)+':',str(binary)+' (architecture arm64):'):
            report.write_text(header+'\n\t/usr/lib/libSystem.B.dylib\n')
            result=subprocess.run(command,cwd=self.repo,text=True,capture_output=True,timeout=10);self.assertEqual(result.returncode,0,result.stderr)
        for dependency in ('/opt/homebrew/lib/bad.dylib','/usr/local/Cellar/bad.dylib',str(binary)):
            report.write_text(str(binary)+':\n\t'+dependency+' (compatibility version 1.0.0)\n')
            result=subprocess.run(command,cwd=self.repo,text=True,capture_output=True,timeout=10);self.assertNotEqual(result.returncode,0)

    def test_contract_inspection_does_not_allocate_release_directory(self):
        result=self.make('release-contract');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertFalse((self.repo/'build').exists())
        self.assertIn('notary_profile_set: no',result.stdout);self.assertIn('team_id_set: no',result.stdout)

    @unittest.skipUnless(Path('/usr/libexec/PlistBuddy').is_file(), 'requires native macOS plist reader')
    def test_actual_launcher_inspection_in_release_recipe_does_not_create_home(self):
        launcher = self.app / 'Contents/MacOS/physics-sim-launcher'
        shutil.copy2(ROOT / 'tools/packaging/macos/physics-sim-launcher', launcher)
        launcher.chmod(0o755)
        (self.app / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier': 'com.fixture.app'}))
        home = self.repo / 'absent-user-home'
        result = subprocess.run(['make', '-j2', 'release-bundle-audit'], cwd=self.repo,
            env=dict(ENV, HOME=str(home)), capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(home.exists())
        receipt = next((self.repo / 'build/release/.package-proofs').glob('*/receipt.json'))
        self.assertEqual(json.loads(receipt.read_text())['state'], 'passed')
        config = (receipt.parent / 'work/print_config.txt').read_text()
        self.assertIn('CONFIG_INSPECTION_READ_ONLY=1', config)
        self.assertIn('VK_ICD_FILENAMES=\n', config)

if __name__=='__main__':unittest.main()
