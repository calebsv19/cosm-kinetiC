"""Exercise real bundler lifecycle/engine with synthetic Mach-O tools only."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import macos_bundle as bundle
from package_outputs import plan, declare


class BundleLifecycle(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='physics-bundler-')
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name).resolve() / 'checkout with spaces'
        self.repo.mkdir()
        for relative in ('scripts/macos_bundle.py', 'scripts/macos_bundle_engine.sh',
                         'tools/packaging/macos/bundle-dylibs.sh'):
            dst = self.repo / relative
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, dst)
        self.root = self.repo / 'dist/attempt'
        self.app = self.root / 'kinetiC Main Edit.app'
        validate = lambda: plan(self.repo, self.root, [self.app], [])
        declare(validate(), validate)
        self.binary = self.app / 'Contents/MacOS/physics-sim-bin'
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(b'original-app')
        self.frameworks = self.app / 'Contents/Frameworks'
        self.frameworks.mkdir()
        self.libroot = self.repo / 'dependencies'
        (self.libroot / 'lib').mkdir(parents=True)
        self.env = patch.dict(os.environ, {'PACKAGE_DEP_SEARCH_ROOTS': str(self.libroot)})
        self.env.start()
        self.addCleanup(self.env.stop)
        for key in ('PHYSICS_SIM_BUILD_OWNER_ROOT', 'PHYSICS_SIM_BUILD_HIERARCHY_FDS'):
            patcher = patch.dict(os.environ)
            patcher.start()
            os.environ.pop(key, None)
            self.addCleanup(patcher.stop)
        self.otool = self.repo / 'otool-stub'
        self.install = self.repo / 'install-stub'
        self.set_tools('printf "%s:\\n" "$2"', 'exit 0')

    def set_tools(self, otool, install):
        for path, code in ((self.otool, otool), (self.install, install)):
            path.write_text('#!/bin/sh\nset -eu\n' + code + '\n')
            path.chmod(0o755)
        engine = (ROOT / 'scripts/macos_bundle_engine.sh').read_text()
        engine = engine.replace('OTOOL_BIN="/usr/bin/otool"', 'OTOOL_BIN="' + str(self.otool) + '"')
        engine = engine.replace('INSTALL_NAME_TOOL_BIN="/usr/bin/install_name_tool"', 'INSTALL_NAME_TOOL_BIN="' + str(self.install) + '"')
        (self.repo / 'scripts/macos_bundle_engine.sh').write_text(engine)

    def run_bundle(self, **kwargs):
        return bundle.run(self.repo, self.binary, self.frameworks, **kwargs)

    def attempts(self):
        return sorted((self.repo / 'data/experiments/package-bundler-attempts').glob('bundle-*'))

    def receipt(self):
        return json.loads((self.attempts()[-1] / 'receipt.json').read_text())

    def dependency(self):
        (self.libroot / 'lib/liba.dylib').write_bytes(b'library-a')
        self.set_tools('printf "%s:\\n" "$2"; case "$2" in */physics-sim-bin) printf "\\t@rpath/liba.dylib (compatibility version 1.0.0, current version 1.0.0)\\n";; esac', 'printf "%s\\n" "$*" >> "' + str(self.repo / 'tool-calls') + '"')

    def test_empty_dependencies_retains_request_snapshot_and_diagnostics(self):
        attempt = self.run_bundle()
        self.assertEqual(self.receipt()['status'], 'passed')
        self.assertEqual((attempt / 'before/binary').read_bytes(), b'original-app')
        self.assertTrue((attempt / 'bundle.stdout').is_file())
        self.assertTrue((attempt / 'work/otool-1.txt').is_file())
        self.assertFalse(self.receipt()['automatic_removal'])
        self.assertFalse(self.receipt()['all_external_descendants_verified_terminal'])

    def test_dependency_copy_and_rewrite_are_functional(self):
        self.dependency()
        self.run_bundle()
        self.assertEqual((self.frameworks / 'liba.dylib').read_bytes(), b'library-a')
        calls = (self.repo / 'tool-calls').read_text()
        self.assertIn('-id @loader_path/liba.dylib', calls)
        self.assertIn('-change @rpath/liba.dylib @executable_path/../Frameworks/liba.dylib', calls)

    def test_repeat_keeps_all_prior_attempt_bytes(self):
        first = self.run_bundle()
        before = {str(p.relative_to(first)): p.read_bytes() for p in first.rglob('*') if p.is_file()}
        second = self.run_bundle()
        self.assertNotEqual(first, second)
        self.assertEqual(before, {str(p.relative_to(first)): p.read_bytes() for p in first.rglob('*') if p.is_file()})

    def test_otool_nonzero_is_failure_with_retained_diagnostic(self):
        self.set_tools('echo diagnostic >&2; exit 17', 'exit 0')
        with self.assertRaisesRegex(ValueError, 'exited 17'):
            self.run_bundle()
        self.assertEqual(self.receipt()['status'], 'failed')
        self.assertIn('diagnostic', (self.attempts()[0] / 'bundle.stderr').read_text())

    def test_install_id_failure_is_not_suppressed(self):
        self.dependency()
        self.install.write_text('#!/bin/sh\necho id-failure >&2\nexit 19\n')
        with self.assertRaisesRegex(ValueError, 'exited 19'):
            self.run_bundle()
        self.assertEqual(self.receipt()['status'], 'failed')
        self.assertTrue((self.attempts()[0] / 'before/binary').is_file())

    def test_install_change_failure_is_not_suppressed(self):
        self.dependency()
        self.install.write_text('#!/bin/sh\n[ "$1" != -change ] || exit 23\n')
        with self.assertRaisesRegex(ValueError, 'exited 23'):
            self.run_bundle()
        self.assertEqual(self.receipt()['status'], 'failed')

    def test_unresolved_rpath_fails_instead_of_publishing_success(self):
        self.set_tools('printf "binary:\\n\\t@rpath/missing.dylib (compatibility version 1.0.0)\\n"', 'exit 0')
        with self.assertRaises(ValueError):
            self.run_bundle()
        self.assertIn('unable to resolve', (self.attempts()[0] / 'bundle.stderr').read_text())

    def test_optional_vulkan_id_failure_is_not_suppressed(self):
        (self.libroot / 'lib/libMoltenVK.dylib').write_bytes(b'optional')
        self.set_tools('printf "binary:\\n"', 'exit 21')
        with self.assertRaisesRegex(ValueError, 'exited 21'):
            self.run_bundle()

    def test_links_in_frameworks_are_rejected_before_attempt(self):
        (self.frameworks / 'linked').symlink_to(self.binary)
        with self.assertRaises(ValueError):
            self.run_bundle()
        self.assertFalse(self.attempts())

    def test_outside_app_and_wrong_frameworks_are_refused(self):
        with self.assertRaises(ValueError):
            bundle.run(self.repo, self.binary, self.repo / 'other')
        with self.assertRaises(ValueError):
            bundle.run(self.repo, self.repo / 'src/fake', self.frameworks)
        self.assertFalse(self.attempts())

    def test_unreserved_app_is_refused(self):
        for p in (self.root / '.package-reservations').iterdir():
            p.unlink()
        with self.assertRaisesRegex(ValueError, 'reservation'):
            self.run_bundle()
        self.assertFalse(self.attempts())

    def test_cleanup_owner_holds_without_mutating_package(self):
        locks = self.repo / 'tmp/locks'
        locks.mkdir(parents=True)
        with (locks / 'clean.lock').open('w') as owner:
            fcntl.flock(owner, fcntl.LOCK_EX)
            with self.assertRaisesRegex(ValueError, 'active cleanup'):
                self.run_bundle()
        self.assertEqual(self.binary.read_bytes(), b'original-app')
        self.assertFalse(self.attempts())

    def test_same_app_owner_holds(self):
        locks = self.root / '.bundler-locks'
        locks.mkdir()
        key = hashlib.sha256(str(self.app).encode()).hexdigest()
        with (locks / (key + '.lock')).open('w') as owner:
            fcntl.flock(owner, fcntl.LOCK_EX)
            with self.assertRaisesRegex(ValueError, 'same-app bundler'):
                self.run_bundle()
        self.assertFalse(self.attempts())

    def test_timeout_retains_failure(self):
        self.set_tools('sleep 10', 'exit 0')
        with self.assertRaisesRegex(ValueError, 'wall cap'):
            self.run_bundle(wall_cap=.1)
        self.assertEqual(self.receipt()['status'], 'failed')

    def test_log_cap_retains_failure(self):
        self.set_tools('echo 123456789012345678901234567890 >&2', 'exit 0')
        with self.assertRaisesRegex(ValueError, 'log cap'):
            self.run_bundle(log_cap=8)
        self.assertEqual(self.receipt()['status'], 'failed')

    def test_legacy_predictable_tmp_is_never_touched(self):
        old = self.repo / 'physicssim_bundle_dylibs.1234'
        old.mkdir()
        marker = old / 'unrelated'
        marker.write_bytes(b'keep')
        with patch.dict(os.environ, {'TMPDIR': str(old)}):
            self.run_bundle()
        self.assertEqual(marker.read_bytes(), b'keep')

    def test_internal_framework_link_is_refused(self):
        (self.frameworks / 'real').write_bytes(b'keep')
        (self.frameworks / 'alias').symlink_to('real')
        with self.assertRaisesRegex(ValueError, 'linked package'):
            self.run_bundle()
        self.assertFalse(self.attempts())

    def test_invalid_limits_refuse_before_allocating(self):
        for limits in ({'wall_cap': float('nan')}, {'wall_cap': True}, {'wall_cap': 0},
                       {'log_cap': True}, {'log_cap': 0}, {'wall_cap': 3601}):
            with self.subTest(limits=limits), self.assertRaisesRegex(ValueError, 'bounds'):
                self.run_bundle(**limits)
        self.assertFalse(self.attempts())

    def test_malformed_reservation_is_refused_before_mutation(self):
        receipt = next((self.root / '.package-reservations').glob('*.json'))
        row = json.loads(receipt.read_text())
        row['release_authority_granted'] = True
        receipt.write_text(json.dumps(row))
        with self.assertRaisesRegex(ValueError, 'reservation'):
            self.run_bundle()
        self.assertFalse(self.attempts())

    def test_control_drift_is_held_before_success(self):
        target = self.repo / 'tools/packaging/macos/bundle-dylibs.sh'
        self.set_tools('printf "binary:\\n"; echo changed >> "' + str(target) + '"', 'exit 0')
        with self.assertRaisesRegex(ValueError, 'control or reservation changed'):
            self.run_bundle()
        self.assertEqual(self.receipt()['status'], 'failed')

    def test_partial_mutation_preserves_binary_and_framework_predecessor(self):
        (self.frameworks / 'existing').write_bytes(b'previous-framework')
        self.dependency()
        self.install.write_text('#!/bin/sh\nprintf mutated > "' + str(self.binary) + '"\nexit 29\n')
        with self.assertRaises(ValueError):
            self.run_bundle()
        attempt = self.attempts()[0]
        self.assertEqual((attempt / 'before/binary').read_bytes(), b'original-app')
        self.assertEqual((attempt / 'before/Frameworks/existing').read_bytes(), b'previous-framework')
        self.assertEqual(self.binary.read_bytes(), b'mutated')
        self.assertFalse(self.receipt()['automatic_rollback_performed'])

    def test_actual_wrapper_cli_works_with_selected_synthetic_tools(self):
        env = dict(os.environ, PYTHONPATH=str(ROOT / 'scripts'))
        result = subprocess.run(['/bin/sh', str(self.repo / 'tools/packaging/macos/bundle-dylibs.sh'),
            str(self.binary), str(self.frameworks)], env=env, capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Bundler retained:', result.stdout)
        self.assertEqual(self.receipt()['status'], 'passed')

    def test_actual_wrapper_argument_failure_creates_no_attempt(self):
        result = subprocess.run(['/bin/sh', str(self.repo / 'tools/packaging/macos/bundle-dylibs.sh')],
            env=dict(os.environ, PYTHONPATH=str(ROOT / 'scripts')), capture_output=True, text=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('required', result.stderr)
        self.assertFalse(self.attempts())

    def test_sealed_attempt_parent_refuses_before_mutation(self):
        parent = self.repo / 'data/experiments/package-bundler-attempts'
        parent.mkdir(parents=True)
        marker = parent / 'bundle_manifest.json'
        marker.write_text('{}')
        with self.assertRaisesRegex(ValueError, 'sealed evidence'):
            self.run_bundle()
        self.assertEqual(marker.read_text(), '{}')
        self.assertFalse(self.attempts())


if __name__ == '__main__':
    unittest.main()
