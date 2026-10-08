"""Portable packaged installer tests; all user directories are disposable fixtures."""
import fcntl
import hashlib
import importlib.util
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
SOURCE = ROOT / 'tools/packaging/linux/install-desktop-entry.py'
spec = importlib.util.spec_from_file_location('desktop_installer', SOURCE)
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class DesktopInstaller(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='physics-desktop-install-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name).resolve()
        self.package = self.base / 'package with spaces'
        self.share = self.package / 'share'
        self.launcher = self.package / 'bin/physics-sim-launcher'
        self.launcher.parent.mkdir(parents=True)
        self.launcher.write_text('#!/bin/sh\nprintf launched\n')
        self.launcher.chmod(0o755)
        self.icon_source = self.share / 'icons/hicolor/scalable/apps/kinetic.svg'
        self.icon_source.parent.mkdir(parents=True)
        self.icon_source.write_bytes(b'<svg>new icon</svg>')
        for name in ('install-desktop-entry.py', 'install-desktop-entry.sh'):
            shutil.copy2(ROOT / 'tools/packaging/linux' / name, self.share / name)
        self.data = self.base / 'user data'
        self.entry = self.data / 'applications/kinetic.desktop'
        self.icons = self.data / 'icons/hicolor/scalable/apps'
        self.store = self.data / 'PhysicsSim/desktop-entry-installs'

    def install(self, **kwargs):
        return installer.install(self.package, self.data, **kwargs)

    def attempts(self):
        return sorted(p for p in self.store.glob('*') if p.is_dir())

    def seed(self):
        self.entry.parent.mkdir(parents=True)
        self.entry.write_bytes(b'old desktop entry')
        self.icons.mkdir(parents=True)
        (self.icons / 'kinetic.svg').write_bytes(b'old icon')
        self.entry.chmod(0o640)

    def tree(self, root):
        return {str(p.relative_to(root)): (p.read_bytes(), p.stat().st_mode & 0o777)
                for p in root.rglob('*') if p.is_file()}

    def cli(self, *args, env=None):
        values = dict(os.environ, HOME=str(self.base / 'home'), XDG_DATA_HOME=str(self.data))
        if env:
            values.update(env)
        return subprocess.run(['/bin/sh', str(self.share / 'install-desktop-entry.sh'), *args],
                              env=values, capture_output=True, text=True, timeout=10)

    def crash(self, stage):
        code = ('import importlib.util,os; from pathlib import Path; '
                's=importlib.util.spec_from_file_location("install",' + repr(str(self.share / 'install-desktop-entry.py')) + '); '
                'm=importlib.util.module_from_spec(s);s.loader.exec_module(m); '
                'm.install(Path(' + repr(str(self.package)) + '),Path(' + repr(str(self.data)) + '),'
                'checkpoint=lambda step: os._exit(73) if step==' + repr(stage) + ' else None)')
        result = subprocess.run([sys.executable, '-B', '-c', code], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 73, result.stderr)
        return self.attempts()[-1]

    def test_first_install_is_usable_quoted_and_points_at_exact_icon_generation(self):
        result = self.install()
        text = self.entry.read_text()
        self.assertIn('Exec="' + str(self.launcher) + '"', text)
        final_icon = self.icons / ('kinetic-' + hashlib.sha256(self.icon_source.read_bytes()).hexdigest() + '.svg')
        self.assertIn('Icon=' + str(final_icon), text)
        self.assertEqual(final_icon.read_bytes(), self.icon_source.read_bytes())
        self.assertEqual(self.entry.stat().st_mode & 0o777, 0o644)
        self.assertEqual(result['status'], 'completed')
        self.assertTrue((self.attempts()[0] / 'completed.json').is_file())
        request = json.loads((self.attempts()[0] / 'request.json').read_text())
        self.assertEqual(request['artifact_class'], 'retained_desktop_install_history')
        self.assertFalse(request['automatic_removal'])

    def test_legacy_entry_and_icon_are_retained_before_replacement(self):
        self.seed()
        self.install()
        attempt = self.attempts()[0]
        self.assertEqual((attempt / 'previous.desktop').read_bytes(), b'old desktop entry')
        self.assertEqual((attempt / 'previous.desktop').stat().st_mode & 0o777, 0o640)
        self.assertEqual((attempt / 'previous-legacy.svg').read_bytes(), b'old icon')
        self.assertEqual((self.icons / 'kinetic.svg').read_bytes(), b'old icon')

    def test_repeat_preserves_all_prior_attempt_bytes_and_icon_generations(self):
        first = self.install()
        before = self.tree(Path(first['attempt']))
        old_icons = self.tree(self.icons)
        self.install()
        self.assertEqual(before, self.tree(Path(first['attempt'])))
        for key, value in old_icons.items():
            self.assertEqual(value, self.tree(self.icons)[key])
        self.assertEqual(len(self.attempts()), 2)

    def test_changed_icon_keeps_old_generation_and_entry_snapshot(self):
        first = self.install()
        old_entry = self.entry.read_bytes()
        old_icon = next(self.icons.glob('kinetic-*.svg'))
        self.icon_source.write_bytes(b'<svg>second icon</svg>')
        second = self.install()
        self.assertNotEqual(first['attempt'], second['attempt'])
        self.assertTrue(old_icon.is_file())
        self.assertEqual((Path(second['attempt']) / 'previous.desktop').read_bytes(), old_entry)
        self.assertNotEqual(self.entry.read_bytes(), old_entry)

    def test_plan_from_absent_data_root_creates_nothing(self):
        result = self.install(plan=True)
        self.assertEqual(result['status'], 'plan')
        self.assertFalse(self.data.exists())

    def test_plan_of_pending_attempt_is_read_only(self):
        attempt = self.crash('after_icon')
        before = self.tree(self.data)
        result = self.install(plan=True)
        self.assertEqual(result['pending_attempts'], [attempt.name])
        self.assertEqual(before, self.tree(self.data))

    def test_crash_recovery_at_all_five_publication_checkpoints(self):
        for stage in ('prepared', 'before_icon', 'after_icon', 'before_entry', 'after_entry'):
            with self.subTest(stage=stage):
                data = self.base / stage
                original = self.data
                self.data = data
                self.entry = data / 'applications/kinetic.desktop'
                self.icons = data / 'icons/hicolor/scalable/apps'
                self.store = data / 'PhysicsSim/desktop-entry-installs'
                self.seed()
                attempt = self.crash(stage)
                if stage != 'after_entry':
                    self.assertEqual(self.entry.read_bytes(), b'old desktop entry')
                else:
                    icon_value = next(line[5:] for line in self.entry.read_text().splitlines() if line.startswith('Icon='))
                    self.assertEqual(Path(icon_value).read_bytes(), self.icon_source.read_bytes())
                with self.assertRaisesRegex(ValueError, 'Unfinished'):
                    self.install()
                result = self.install(recover=attempt.name)
                self.assertEqual(result['status'], 'completed')
                self.assertTrue((attempt / 'completed.json').is_file())
                self.assertEqual((self.icons / 'kinetic.svg').read_bytes(), b'old icon')
                self.data = original

    def test_changed_entry_after_interruption_is_never_replaced(self):
        self.seed()
        attempt = self.crash('after_icon')
        self.entry.write_bytes(b'user edit after crash')
        with self.assertRaisesRegex(ValueError, 'changed since'):
            self.install(recover=attempt.name)
        self.assertEqual(self.entry.read_bytes(), b'user edit after crash')
        self.assertFalse((attempt / 'completed.json').exists())
        self.assertTrue(list(attempt.glob('failure-*.json')))

    def test_source_drift_blocks_recovery(self):
        self.seed()
        attempt = self.crash('prepared')
        self.launcher.write_text('#!/bin/sh\nexit 3\n')
        with self.assertRaisesRegex(ValueError, 'source/path mismatch'):
            self.install(recover=attempt.name)
        self.assertEqual(self.entry.read_bytes(), b'old desktop entry')

    def test_candidate_tamper_blocks_recovery(self):
        attempt = self.crash('prepared')
        (attempt / 'desired.desktop').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'candidate changed'):
            self.install(recover=attempt.name)
        self.assertFalse(self.entry.exists())

    def test_predecessor_snapshot_tamper_blocks_recovery(self):
        self.seed()
        attempt = self.crash('prepared')
        (attempt / 'previous.desktop').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'snapshot mismatch'):
            self.install(recover=attempt.name)
        self.assertEqual(self.entry.read_bytes(), b'old desktop entry')

    def test_existing_generation_collision_is_held_without_mutation(self):
        self.icons.mkdir(parents=True)
        final_icon = self.icons / ('kinetic-' + hashlib.sha256(self.icon_source.read_bytes()).hexdigest() + '.svg')
        final_icon.write_bytes(b'foreign collision')
        with self.assertRaisesRegex(ValueError, 'generation differs'):
            self.install()
        self.assertEqual(final_icon.read_bytes(), b'foreign collision')
        self.assertFalse(self.store.exists())

    def test_linked_data_root_entry_icon_and_store_are_refused(self):
        foreign = self.base / 'foreign'
        foreign.mkdir()
        for selected in ('root', 'entry', 'icon', 'store'):
            with self.subTest(selected=selected):
                root = self.base / selected
                if selected == 'root':
                    root.symlink_to(foreign, target_is_directory=True)
                elif selected == 'entry':
                    (root / 'applications').mkdir(parents=True)
                    (root / 'applications/kinetic.desktop').symlink_to(foreign / 'file')
                elif selected == 'icon':
                    (root / 'icons/hicolor/scalable/apps').mkdir(parents=True)
                    (root / 'icons/hicolor/scalable/apps/kinetic.svg').symlink_to(foreign / 'file')
                else:
                    (root / 'PhysicsSim').mkdir(parents=True)
                    (root / 'PhysicsSim/desktop-entry-installs').symlink_to(foreign, target_is_directory=True)
                with self.assertRaisesRegex(ValueError, 'linked path'):
                    installer.install(self.package, root)
                self.assertEqual(list(foreign.iterdir()), [])

    def test_special_destination_is_refused_without_blocking(self):
        self.entry.parent.mkdir(parents=True)
        os.mkfifo(self.entry)
        with self.assertRaisesRegex(ValueError, 'type/size'):
            self.install()
        self.assertFalse(self.store.exists())

    def test_invalid_recovery_ids_do_not_allocate(self):
        for value in ('../escape', '', 'A' * 32, 'a' * 33):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, '32-hex'):
                self.install(recover=value)
        self.assertFalse(self.store.exists())

    def test_competing_owner_refuses(self):
        self.store.mkdir(parents=True)
        with (self.store / 'owner.lock').open('wb') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            with self.assertRaisesRegex(ValueError, 'Another desktop installer'):
                self.install()
        self.assertFalse(self.entry.exists())
        self.assertFalse(self.attempts())

    def test_oversized_source_and_predecessor_refuse_before_allocation(self):
        self.icon_source.write_bytes(b'x' * (installer.LIMIT + 1))
        with self.assertRaisesRegex(ValueError, 'type/size'):
            self.install()
        self.assertFalse(self.store.exists())
        self.icon_source.write_bytes(b'icon')
        self.entry.parent.mkdir(parents=True)
        self.entry.write_bytes(b'x' * (installer.LIMIT + 1))
        with self.assertRaisesRegex(ValueError, 'type/size'):
            self.install()
        self.assertFalse(self.store.exists())

    def test_unknown_history_is_held_without_deletion(self):
        self.store.mkdir(parents=True)
        marker = self.store / 'unknown'
        marker.write_bytes(b'keep')
        with self.assertRaisesRegex(ValueError, 'Unknown installer history'):
            self.install()
        self.assertEqual(marker.read_bytes(), b'keep')
        self.assertFalse(self.entry.exists())

    def test_completed_recovery_is_read_only_for_exact_current_install(self):
        result = self.install()
        before = self.tree(self.data)
        repeated = self.install(recover=Path(result['attempt']).name)
        self.assertEqual(repeated['status'], 'already_completed')
        self.assertEqual(before, self.tree(self.data))

    def test_completed_recovery_does_not_claim_changed_entry_is_current(self):
        result = self.install()
        self.entry.write_bytes(b'user edit')
        with self.assertRaisesRegex(ValueError, 'no longer matches'):
            self.install(recover=Path(result['attempt']).name)
        self.assertEqual(self.entry.read_bytes(), b'user edit')

    def test_packaged_wrapper_plan_install_and_help_work_without_source_tree(self):
        result = self.cli('--plan')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['status'], 'plan')
        self.assertFalse(self.data.exists())
        result = self.cli('--help')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.data.exists())
        result = self.cli()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), str(self.entry))
        self.assertTrue(self.entry.is_file())

    def test_packaged_wrapper_unknown_argument_does_not_install(self):
        result = self.cli('--force')
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.data.exists())

    def test_default_data_home_and_empty_xdg_use_home_local_share(self):
        result = self.cli('--plan', env={'XDG_DATA_HOME': ''})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['entry'], str(self.base / 'home/.local/share/applications/kinetic.desktop'))
        self.assertFalse((self.base / 'home').exists())

    def test_exec_quoting_covers_desktop_string_and_argument_layers(self):
        text = installer.desktop(Path('/a b/"quote"/$cash/`tick`/back\\slash/100%/launcher'), Path('/icons/back\\slash.svg')).decode()
        self.assertIn(r'Exec="/a b/\\"quote\\"/\\$cash/\\`tick\\`/back\\\\slash/100%%/launcher"', text)
        self.assertIn(r'Icon=/icons/back\\slash.svg', text)
        with self.assertRaisesRegex(ValueError, 'ASCII without equals'):
            installer.desktop(Path('/equal=sign/launcher'), Path('/icon'))

    def test_package_overlap_and_relative_data_root_are_refused(self):
        for root in (self.package, self.package / 'data', Path('relative'), Path('/')):
            with self.subTest(root=root), self.assertRaises(ValueError):
                installer.install(self.package, root)
        self.assertFalse(self.store.exists())

    def test_installer_control_drift_blocks_recovery(self):
        attempt = self.crash('prepared')
        with (self.share / 'install-desktop-entry.py').open('a') as stream:
            stream.write('# changed\n')
        with self.assertRaisesRegex(ValueError, 'installer_identity'):
            self.install(recover=attempt.name)
        self.assertFalse(self.entry.exists())

    def test_entry_publication_error_preserves_previous_entry_and_legacy_icon(self):
        self.seed()
        with patch.object(installer.os, 'replace', side_effect=OSError('synthetic publication failure')):
            with self.assertRaisesRegex(OSError, 'synthetic publication'):
                self.install()
        self.assertEqual(self.entry.read_bytes(), b'old desktop entry')
        self.assertEqual((self.icons / 'kinetic.svg').read_bytes(), b'old icon')
        attempt = self.attempts()[0]
        self.assertTrue(list(attempt.glob('failure-*.json')))
        self.assertFalse((attempt / 'completed.json').exists())
        self.install(recover=attempt.name)
        self.assertTrue((attempt / 'completed.json').is_file())

    def test_record_duplicate_keys_and_nonfinite_values_are_rejected(self):
        attempt = self.crash('prepared')
        request = attempt / 'request.json'
        original = request.read_bytes()
        for value in (b'{"schema": "one", "schema": "two"}', b'{"schema": NaN}'):
            with self.subTest(value=value):
                request.write_bytes(value)
                with self.assertRaises(ValueError):
                    self.install(recover=attempt.name)
                self.assertFalse(self.entry.exists())
        request.write_bytes(original)
        self.install(recover=attempt.name)

    def test_competing_edit_at_final_boundary_is_preserved(self):
        self.seed()
        def edit(stage):
            if stage == 'before_entry':
                self.entry.write_bytes(b'concurrent user edit')
        with self.assertRaisesRegex(ValueError, 'changed at publication'):
            self.install(checkpoint=edit)
        self.assertEqual(self.entry.read_bytes(), b'concurrent user edit')
        self.assertEqual((self.icons / 'kinetic.svg').read_bytes(), b'old icon')

    def test_actual_package_recipe_copies_both_standalone_installer_files(self):
        make_source = (ROOT / 'make/package-linux-desktop.mk').read_text().splitlines()
        selected = [line for line in make_source if line.startswith(('LINUX_DESKTOP_INSTALLER_SRC :=', 'LINUX_DESKTOP_INSTALLER_HELPER_SRC :='))]
        copies = [line for line in make_source if line.startswith('\t@cp "$(LINUX_DESKTOP_INSTALLER')]
        self.assertEqual(len(copies), 2)
        destination = self.base / 'recipe package/share'
        destination.mkdir(parents=True)
        makefile = self.base / 'installer-recipe.mk'
        makefile.write_text('\n'.join(selected) + '\nLINUX_DESKTOP_SHARE_DIR := ' + str(destination) +
            '\nLINUX_DESKTOP_INSTALLER := $(LINUX_DESKTOP_SHARE_DIR)/install-desktop-entry.sh\ncopy-installer:\n' + '\n'.join(copies) + '\n')
        env = {k: v for k, v in os.environ.items() if k not in ('MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES')}
        result = subprocess.run(['make', '-f', str(makefile), 'copy-installer'], cwd=ROOT, env=env,
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in ('install-desktop-entry.sh', 'install-desktop-entry.py'):
            self.assertEqual((destination / name).read_bytes(), (self.share / name).read_bytes())

    def test_legacy_icon_snapshot_tamper_blocks_recovery(self):
        self.seed()
        attempt = self.crash('prepared')
        (attempt / 'previous-legacy.svg').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'legacy icon snapshot mismatch'):
            self.install(recover=attempt.name)
        self.assertEqual((self.icons / 'kinetic.svg').read_bytes(), b'old icon')

    def test_missing_packaged_control_refuses_before_allocation(self):
        (self.share / 'install-desktop-entry.sh').unlink()
        with self.assertRaises(FileNotFoundError):
            self.install()
        self.assertFalse(self.store.exists())

    def test_completed_entry_mode_drift_is_not_claimed_current(self):
        result = self.install()
        self.entry.chmod(0o640)
        with self.assertRaisesRegex(ValueError, 'no longer matches'):
            self.install(recover=Path(result['attempt']).name)
        self.assertEqual(self.entry.stat().st_mode & 0o777, 0o640)


if __name__ == '__main__':
    unittest.main()
