"""Copy-on-transform app stages with fake signing/stapling tools only."""
import json
import os
from pathlib import Path
import plistlib
import subprocess
import signal
import time
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from desktop_replace import inventory
from release_app_stage import stage
from package_transaction import recover
from release_notary_archive import prepare
from release_notary import run as notarize
from unittest.mock import patch


class AppStage(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name).resolve()
        self.app = self.repo / 'dist/Fake App.app'
        macos = self.app / 'Contents/MacOS'; macos.mkdir(parents=True)
        for name in ('physics-sim-bin', 'physics_sim_session_worker', 'physics-sim-launcher'):
            (macos / name).write_bytes(b'fixture binary')
        with (self.app / 'Contents/Info.plist').open('wb') as stream:
            plistlib.dump({'CFBundleIdentifier': 'fixture.app'}, stream)
        for folder in ('first folder', 'second folder'):
            file = self.app / 'Contents/Frameworks' / folder / 'same name.dylib'
            file.parent.mkdir(parents=True); file.write_bytes(b'fixture dylib')
        (self.app / 'Contents/Frameworks/alias.dylib').symlink_to('first folder/same name.dylib')
        self.tool = self.repo / 'fake-tool'
        self.journal = self.repo / 'commands.jsonl'
        self.tool.write_text('#!' + sys.executable + '\n' +
            'import json,sys\nfrom pathlib import Path\n' +
            'with Path(' + repr(str(self.journal)) + ').open("a") as f:f.write(json.dumps(sys.argv[1:])+"\\n")\n' +
            'if "--force" in sys.argv:\n p=Path(sys.argv[-1]);p=p/"signed.marker" if p.is_dir() else p;p.write_bytes(p.read_bytes()+b" signed" if p.exists() else b"signed")\n' +
            'if "staple" in sys.argv:(Path(sys.argv[-1])/"stapled.marker").write_bytes(b"stapled")\n' +
            'if "-k" in sys.argv:\n' +
            ' import zipfile,os\n src=Path(sys.argv[-2])\n' +
            ' with zipfile.ZipFile(sys.argv[-1],"w",zipfile.ZIP_DEFLATED) as z:\n' +
            '  for p in [src]+sorted(src.rglob("*")):\n' +
            '   name=str(p.relative_to(src.parent))\n' +
            '   if p.is_symlink():\n    i=zipfile.ZipInfo(name);i.create_system=3;i.external_attr=p.lstat().st_mode<<16;z.writestr(i,os.fsencode(os.readlink(p)))\n' +
            '   else:z.write(p,name)\n' +
            'if "notarytool" in sys.argv:print(json.dumps({"id":"12345678-1234-4234-8234-123456789abc","status":"Accepted"}))\n')
        self.tool.chmod(0o755)
        self.root = self.repo / 'build/sign stage'
        self.before = inventory(self.app)

    def run_stage(self, **options):
        return stage(self.repo, self.app, self.root, 'fixture.app', 'sign', str(self.tool), **options)

    def acceptance(self):
        archive_root=self.repo/'build/notary archive'
        result=prepare(self.repo,self.receipt(),archive_root,str(self.tool))
        journal=self.repo/'build/notary journal'
        args=(self.repo,archive_root/'notary-upload.zip',journal,Path(result['receipt']),'fixture profile',str(self.tool))
        notarize(*args);notarize(*args,reconcile=True)
        return journal/'receipt.json'

    def receipt(self):
        return next((self.root / '.package-transactions').glob('*/receipt.json'))

    def commands(self):
        return [json.loads(row) for row in self.journal.read_text().splitlines()]

    def test_sign_copy_handles_spaces_aliases_and_reuses_without_mutating_source(self):
        self.assertEqual(self.run_stage()['status'], 'completed')
        commands = self.commands(); self.assertEqual(len(commands), 7)
        self.assertIn('first folder/same name.dylib', commands[0][-1])
        self.assertIn('second folder/same name.dylib', commands[1][-1])
        self.assertFalse(any('alias.dylib' in row[-1] for row in commands))
        self.assertIn('--timestamp=none', commands[0])
        self.assertEqual(inventory(self.app), self.before)
        self.assertEqual(self.run_stage()['status'], 'verified_reuse')
        self.assertEqual(self.commands(), commands)
        row = json.loads(self.receipt().read_text())
        self.assertEqual(row['state'], 'completed'); self.assertTrue(row['terminal_processes_verified'])

    def test_first_sign_failure_stops_and_retains_partial_copy_without_publication(self):
        with self.tool.open('a') as stream: stream.write('raise SystemExit(7)\n')
        with self.assertRaisesRegex(ValueError, 'exited'): self.run_stage()
        self.assertEqual(len(self.commands()), 1)
        self.assertFalse((self.root / self.app.name).exists())
        row = json.loads(self.receipt().read_text()); self.assertEqual(row['state'], 'failed_retained')
        self.assertEqual(row['published'], [])
        self.assertTrue((self.receipt().parent / 'stage' / self.app.name).is_dir())
        self.assertEqual(inventory(self.app), self.before)

    def test_verification_failure_prevents_publication(self):
        with self.tool.open('a') as stream: stream.write('if "--verify" in sys.argv:raise SystemExit(9)\n')
        with self.assertRaises(ValueError): self.run_stage()
        self.assertEqual(len(self.commands()), 7)
        self.assertFalse((self.root / self.app.name).exists())
        self.assertEqual(inventory(self.app), self.before)

    def test_developer_id_flags_preserve_argument_boundaries(self):
        self.run_stage(identity='Developer ID: Fixture Owner')
        commands = self.commands()
        self.assertIn('Developer ID: Fixture Owner', commands[0])
        self.assertIn('--timestamp', commands[0]); self.assertNotIn('--options', commands[0])
        self.assertIn('--options', commands[2]); self.assertIn('runtime', commands[2])

    def test_staple_copy_preserves_signed_predecessor_and_validates(self):
        self.run_stage(identity='Developer ID: Fixture'); signed = self.root / self.app.name; before = inventory(signed)
        acceptance=self.acceptance()
        staple_root = self.repo / 'build/staple stage'
        result = stage(self.repo, signed, staple_root, 'fixture.app', 'staple', str(self.tool),notary_receipt=acceptance)
        self.assertEqual(result['status'], 'completed')
        self.assertEqual(inventory(signed), before)
        self.assertTrue((staple_root / self.app.name / 'stapled.marker').exists())
        self.assertEqual(self.commands()[-2][0:2], ['stapler', 'staple'])
        self.assertEqual(self.commands()[-1][0:2], ['stapler', 'validate'])

    def test_staple_validation_failure_retains_unpublished_stage(self):
        with self.tool.open('a') as stream: stream.write('if "validate" in sys.argv:raise SystemExit(11)\n')
        self.run_stage(identity='Developer ID: Fixture');acceptance=self.acceptance()
        staple_root=self.repo/'build/staple failure'
        with self.assertRaises(ValueError):
            stage(self.repo, self.root/self.app.name, staple_root, 'fixture.app', 'staple', str(self.tool),notary_receipt=acceptance)
        self.assertFalse((staple_root / self.app.name).exists())
        self.assertEqual(inventory(self.app), self.before)

    def test_missing_binary_escape_and_overlap_hold_before_tool_execution(self):
        for root in (self.app, self.app / 'nested', self.repo / 'src'):
            with self.assertRaises(ValueError):
                stage(self.repo, self.app, root, 'fixture.app', 'sign', str(self.tool))
        self.assertFalse(self.journal.exists())
        file = self.app / 'Contents/MacOS/physics_sim_session_worker'; file.unlink()
        with self.assertRaisesRegex(ValueError, 'regular binary'): self.run_stage()
        self.assertFalse(self.root.exists()); self.assertFalse(self.journal.exists())

    @unittest.skipUnless(sys.platform == 'darwin', 'macOS bundle metadata')
    def test_resource_fork_and_extended_attribute_survive_copy_and_publication(self):
        binary = self.app / 'Contents/MacOS/physics-sim-bin'
        for name,value in (('com.apple.ResourceFork','fixture resource fork'),('com.codework.lifecycle','fixture attribute')):
            subprocess.run(['/usr/bin/xattr','-w',name,value,str(binary)],check=True,capture_output=True)
        self.run_stage()
        for base in (self.root, self.receipt().parent / 'stage'):
            copied = base / self.app.name / 'Contents/MacOS/physics-sim-bin'
            for name,value in (('com.apple.ResourceFork',b'fixture resource fork'),('com.codework.lifecycle',b'fixture attribute')):
                result=subprocess.run(['/usr/bin/xattr','-p',name,str(copied)],check=True,capture_output=True)
                self.assertEqual(result.stdout.rstrip(b'\n'),value)
        self.assertEqual(subprocess.run(['/usr/bin/xattr','-p','com.apple.ResourceFork',str(binary)],check=True,capture_output=True).stdout.rstrip(b'\n'),b'fixture resource fork')
        self.root = self.repo / 'build/recovery metadata'
        with patch('package_transaction.rename_exclusive',side_effect=OSError('publication interrupted')):
            with self.assertRaises(OSError):self.run_stage()
        attempt = self.receipt().parent
        self.assertEqual(recover(self.repo,attempt,apply=True)['status'],'completed')
        copied=self.root/self.app.name/'Contents/MacOS/physics-sim-bin'
        self.assertEqual(subprocess.run(['/usr/bin/xattr','-p','com.apple.ResourceFork',str(copied)],check=True,capture_output=True).stdout.rstrip(b'\n'),b'fixture resource fork')

    def test_cli_sigterm_reaps_nested_signing_tool_and_retains_failed_attempt(self):
        pidfile = self.repo / 'tool.pid'
        with self.tool.open('a') as stream:
            stream.write('import os,time\nPath(' + repr(str(pidfile)) + ').write_text(str(os.getpid()))\ntime.sleep(30)\n')
        process = subprocess.Popen([sys.executable, '-B', str(ROOT / 'scripts/release_app_stage.py'),
            'sign', '--source', str(self.app), '--root', str(self.root),
            '--bundle-id', 'fixture.app', '--tool', str(self.tool)],
            cwd=self.repo, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        pid = None
        try:
            deadline = time.monotonic() + 10
            while not pidfile.exists() and time.monotonic() < deadline:
                if process.poll() is not None: break
                time.sleep(.05)
            self.assertTrue(pidfile.exists(), 'Signing tool did not start')
            pid = int(pidfile.read_text())
            process.send_signal(signal.SIGTERM)
            out, err = process.communicate(timeout=12)
            self.assertNotEqual(process.returncode, 0, out + err)
            with self.assertRaises(ProcessLookupError): os.kill(pid, 0)
            row = json.loads(self.receipt().read_text())
            self.assertEqual(row['state'], 'failed_retained')
            self.assertTrue(row['terminal_processes_verified'])
            self.assertEqual(row['published'], [])
            self.assertEqual(inventory(self.app), self.before)
        finally:
            if process.poll() is None: process.kill(); process.communicate(timeout=5)
            if pid is not None:
                try: os.kill(pid, signal.SIGKILL)
                except ProcessLookupError: pass

    def test_unknown_predecessor_and_changed_inputs_are_never_replaced(self):
        self.run_stage(); destination = self.root / self.app.name; before = inventory(destination)
        (self.app / 'Contents/MacOS/physics-sim-bin').write_bytes(b'drift')
        with self.assertRaisesRegex(ValueError, 'inputs changed'): self.run_stage()
        self.assertEqual(inventory(destination), before)
        fresh = self.repo / 'build/unknown'; (fresh / self.app.name).mkdir(parents=True)
        (fresh / self.app.name / 'sentinel').write_bytes(b'preserved')
        with self.assertRaises(ValueError):
            stage(self.repo, self.app, fresh, 'fixture.app', 'sign', str(self.tool))
        self.assertEqual((fresh / self.app.name / 'sentinel').read_bytes(), b'preserved')


if __name__ == '__main__': unittest.main()
