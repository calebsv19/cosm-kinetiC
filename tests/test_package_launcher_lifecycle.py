"""Actual packaged shell launchers with disposable resources and stub apps only."""
import json
import os
from pathlib import Path
import plistlib
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAC_TOOLS = Path('/usr/libexec/PlistBuddy').is_file() and Path('/usr/bin/plutil').is_file()
PLATFORMS = ('linux', 'macos') if MAC_TOOLS else ('linux',)


class LauncherLifecycle(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='physics-launcher-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name).resolve()
        self.fixtures = {}
        for platform in PLATFORMS:
            base = self.base / platform
            package = base / ('kinetiC Fixture.app' if platform == 'macos' else 'package with spaces')
            binaries = package / ('Contents/MacOS' if platform == 'macos' else 'bin')
            resources = package / ('Contents/Resources' if platform == 'macos' else 'resources')
            binaries.mkdir(parents=True)
            for relative, data in {'config/app.json': b'{}', 'config/custom_preset.txt': b'fixture',
                    'config/structural_scene.txt': b'fixture', 'config/objects/Hexagon.asset.json': b'{}',
                    'vk_renderer/shaders/textured.vert.spv': b'shader', 'shaders/textured.vert.spv': b'shader',
                    'shared/fixture.txt': b'fixture'}.items():
                p = resources / relative
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(data)
            launcher = binaries / 'physics-sim-launcher'
            shutil.copy2(ROOT / 'tools/packaging' / platform / 'physics-sim-launcher', launcher)
            launcher.chmod(0o755)
            app = binaries / 'physics-sim-bin'
            app.write_text('#!/bin/sh\nprintf "%s\\n" "$PWD" "$@" > "$LAUNCH_FIXTURE_RESULT"\nprintf "stub-app-output\\n"\n')
            app.chmod(0o755)
            if platform == 'macos':
                info = {'CFBundleIdentifier': 'com.cosm.kinetic.fixture', 'PhysicsSimPackageProfile': 'fixture',
                        'PhysicsSimRuntimeNamespace': 'PhysicsSim-Fixture', 'PhysicsSimLogNamespace': 'PhysicsSim-Fixture',
                        'PhysicsSimBuildLabel': 'kinetiC Fixture'}
                (package / 'Contents/Info.plist').write_bytes(plistlib.dumps(info))
                frameworks = package / 'Contents/Frameworks'
                frameworks.mkdir()
                (frameworks / 'libMoltenVK.dylib').write_bytes(b'not-an-executed-library')
            self.fixtures[platform] = {'base': base, 'package': package, 'bin': binaries,
                'resources': resources, 'launcher': launcher, 'home': base / 'home',
                'runtime': base / 'runtime', 'logs': base / 'logs', 'tmp': base / 'tmp',
                'result': base / 'app-result'}

    def invoke(self, platform, *args, changes=None, defaults=False):
        f = self.fixtures[platform]
        env = {k: v for k, v in os.environ.items() if not k.startswith(('PHYSICS_SIM_', 'VK_', 'XDG_', 'SHAPE_ASSET_'))}
        env.update(HOME=str(f['home']), TMPDIR=str(f['tmp']), LAUNCH_FIXTURE_RESULT=str(f['result']))
        if not defaults:
            env.update(PHYSICS_SIM_RUNTIME_DIR=str(f['runtime']), PHYSICS_SIM_LOG_DIR=str(f['logs']))
        if changes:
            env.update({key: str(value) for key, value in changes.items()})
        return subprocess.run(['/bin/sh', str(f['launcher']), *args], env=env, capture_output=True, text=True, timeout=10)

    def tree(self, root):
        result = {}
        for p in root.rglob('*'):
            row = p.lstat()
            value = (row.st_ino, row.st_mode, row.st_mtime_ns)
            if p.is_symlink():
                value += ('symlink', os.readlink(p))
            elif p.is_file():
                value += ('file', p.read_bytes())
            else:
                value += ('directory',)
            result[str(p.relative_to(root))] = value
        return result

    def config(self, result):
        self.assertEqual(result.returncode, 0, result.stderr)
        return dict(line.split('=', 1) for line in result.stdout.splitlines())

    def test_print_config_from_empty_defaults_writes_nothing(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                before = self.tree(f['base'])
                values = self.config(self.invoke(platform, '--print-config', defaults=True))
                self.assertEqual(values['CONFIG_INSPECTION_READ_ONLY'], '1')
                self.assertEqual(values['CONFIG_PATH_SCOPE'], 'requested_without_writability_probe')
                self.assertEqual(before, self.tree(f['base']))
                self.assertFalse(f['home'].exists())
                self.assertFalse(f['tmp'].exists())

    def test_repeated_inspection_preserves_existing_bytes_modes_mtimes_and_inodes(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                f['runtime'].mkdir()
                (f['runtime'] / 'state').write_bytes(b'user-state')
                f['logs'].mkdir()
                (f['logs'] / 'launcher.log').write_bytes(b'old-mac-log')
                (f['logs'] / 'package_launcher.log').write_bytes(b'old-linux-log')
                before = self.tree(f['base'])
                for _ in range(2):
                    self.config(self.invoke(platform, '--print-config'))
                self.assertEqual(before, self.tree(f['base']))

    def test_inspection_of_linked_runtime_and_log_paths_has_no_effect(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                foreign = f['base'] / 'foreign'
                foreign.mkdir()
                (foreign / 'state').write_bytes(b'keep')
                f['runtime'].symlink_to(foreign, target_is_directory=True)
                f['logs'].symlink_to(foreign, target_is_directory=True)
                before = self.tree(f['base'])
                self.config(self.invoke(platform, '--print-config'))
                self.assertEqual(before, self.tree(f['base']))

    def test_inspection_reports_requested_blocked_path_without_temp_fallback(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                f['runtime'].write_bytes(b'not-directory')
                before = self.tree(f['base'])
                values = self.config(self.invoke(platform, '--print-config'))
                self.assertEqual(values['PHYSICS_SIM_RUNTIME_DIR'], str(f['runtime']))
                self.assertEqual(before, self.tree(f['base']))
                self.assertFalse(f['tmp'].exists())

    def test_default_namespaces_and_linux_xdg_roots_are_reported(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                values = self.config(self.invoke(platform, '--print-config', defaults=True))
                expected = f['home'] / ('Library/Application Support/PhysicsSim-Fixture/runtime' if platform == 'macos' else '.local/share/PhysicsSim/runtime')
                self.assertEqual(values['PHYSICS_SIM_RUNTIME_DIR'], str(expected))
                if platform == 'macos':
                    self.assertEqual(values['PACKAGE_PROFILE'], 'fixture')
                    self.assertEqual(values['LOG_NAMESPACE'], 'PhysicsSim-Fixture')
                    self.assertEqual(values['VK_ICD_FILENAMES'], '')
                    self.assertEqual(values['VK_DRIVER_FILES'], '')

    def test_environment_overrides_and_backslashes_print_exactly(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                changes = {'PHYSICS_SIM_RUNTIME_DIR': str(self.fixtures[platform]['base'] / r'back\slash'),
                    'VK_RENDERER_SHADER_ROOT': '/a renderer root', 'SHAPE_ASSET_DIR': '/a shape root',
                    'PHYSICS_SIM_TIMER_HUD': '1', 'PHYSICS_SIM_TIMER_HUD_OVERLAY': '1', 'PHYSICS_SIM_TIMER_HUD_VISUAL_MODE': 'quiet'}
                values = self.config(self.invoke(platform, '--print-config', changes=changes))
                for key, value in changes.items():
                    self.assertEqual(values[key], value)

    def test_self_test_and_stub_launch_still_work(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                result = self.invoke(platform, '--self-test')
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('self-test: ok', result.stdout)
                self.assertFalse(f['result'].exists())
                result = self.invoke(platform, '--fixture-argument', 'value with spaces')
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(f['result'].read_text().splitlines(), [str(f['runtime']), '--fixture-argument', 'value with spaces'])
                log = f['logs'] / ('launcher.log' if platform == 'macos' else 'package_launcher.log')
                self.assertIn('stub-app-output', log.read_text())

    def test_linked_runtime_is_refused_before_logs_are_created(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                foreign = f['base'] / 'foreign'
                foreign.mkdir()
                f['runtime'].symlink_to(foreign, target_is_directory=True)
                result = self.invoke(platform, '--self-test')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('linked destination', result.stderr)
                self.assertEqual(list(foreign.iterdir()), [])
                self.assertFalse(f['logs'].exists())
                self.assertFalse(f['tmp'].exists())

    def test_linked_runtime_data_descendant_is_refused_before_other_writes(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                foreign = f['base'] / 'foreign'
                foreign.mkdir()
                f['runtime'].mkdir()
                (f['runtime'] / 'data').symlink_to(foreign, target_is_directory=True)
                before = self.tree(f['base'])
                result = self.invoke(platform, '--self-test')
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(before, self.tree(f['base']))

    def test_linked_log_directory_or_leaf_is_refused_without_runtime_creation(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                foreign = f['base'] / 'foreign'
                foreign.mkdir()
                f['logs'].symlink_to(foreign, target_is_directory=True)
                result = self.invoke(platform, '--self-test')
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(f['runtime'].exists())
                f['logs'].unlink()
                f['logs'].mkdir()
                target = foreign / 'old-log'
                target.write_bytes(b'keep')
                log = f['logs'] / ('launcher.log' if platform == 'macos' else 'package_launcher.log')
                log.symlink_to(target)
                result = self.invoke(platform, '--self-test')
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(target.read_bytes(), b'keep')
                self.assertFalse(f['runtime'].exists())

    def test_fifo_log_is_rejected_without_blocking(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                f['logs'].mkdir()
                os.mkfifo(f['logs'] / ('launcher.log' if platform == 'macos' else 'package_launcher.log'))
                result = self.invoke(platform, '--self-test')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('not regular', result.stderr)
                self.assertFalse(f['runtime'].exists())

    def test_blocked_runtime_and_log_paths_never_fall_back(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                f['runtime'].write_bytes(b'keep')
                result = self.invoke(platform, '--self-test')
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(f['runtime'].read_bytes(), b'keep')
                self.assertFalse(f['tmp'].exists())
                self.assertFalse(f['logs'].exists())
                f['runtime'].unlink()
                f['logs'].write_bytes(b'keep-log-path')
                result = self.invoke(platform, '--self-test')
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(f['runtime'].exists())
                self.assertFalse(f['tmp'].exists())

    def test_relative_ambiguous_and_control_character_paths_are_refused(self):
        for platform in PLATFORMS:
            for value in ('relative', '/', '/one/../two', '/one/./two', '/one//two', str(self.fixtures[platform]['base'] / 'newline\npath')):
                with self.subTest(platform=platform, value=value):
                    result = self.invoke(platform, '--self-test', changes={'PHYSICS_SIM_RUNTIME_DIR': value})
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse(self.fixtures[platform]['logs'].exists())
                    self.assertFalse(self.fixtures[platform]['tmp'].exists())

    def test_package_overlapping_runtime_or_logs_are_refused(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                f = self.fixtures[platform]
                before = self.tree(f['package'])
                for key in ('PHYSICS_SIM_RUNTIME_DIR', 'PHYSICS_SIM_LOG_DIR'):
                    result = self.invoke(platform, '--self-test', changes={key: str(f['package'] / 'unsafe-state')})
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('overlaps package', result.stderr)
                self.assertEqual(before, self.tree(f['package']))

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_icd_generation_preserves_fixed_and_previous_generations(self):
        f = self.fixtures['macos']
        vk = f['runtime'] / 'vk'
        vk.mkdir(parents=True)
        old = vk / 'MoltenVK_icd.json'
        old.write_bytes(b'previous-fixed-icd')
        first = self.invoke('macos', '--self-test')
        self.assertEqual(first.returncode, 0, first.stderr)
        path1 = Path(self.config_values(first)['VK_ICD_FILENAMES'])
        self.assertEqual(json.loads(path1.read_text())['ICD']['library_path'], str(f['package'] / 'Contents/Frameworks/libMoltenVK.dylib'))
        original = path1.read_bytes()
        second = self.invoke('macos', '--self-test')
        self.assertEqual(second.returncode, 0, second.stderr)
        path2 = Path(self.config_values(second)['VK_ICD_FILENAMES'])
        self.assertNotEqual(path1, path2)
        self.assertEqual(path1.read_bytes(), original)
        self.assertEqual(old.read_bytes(), b'previous-fixed-icd')
        before = self.tree(f['base'])
        self.config(self.invoke('macos', '--print-config'))
        self.assertEqual(before, self.tree(f['base']))

    def config_values(self, result):
        return dict(line.split('=', 1) for line in result.stdout.splitlines() if '=' in line)

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_explicit_driver_overrides_do_not_create_unused_icd(self):
        f = self.fixtures['macos']
        result = self.invoke('macos', '--self-test', changes={'VK_ICD_FILENAMES': '/operator/icd', 'VK_DRIVER_FILES': '/operator/driver'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((f['runtime'] / 'vk').exists())
        values = self.config_values(result)
        self.assertEqual(values['VK_ICD_FILENAMES'], '/operator/icd')
        self.assertEqual(values['VK_DRIVER_FILES'], '/operator/driver')

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_one_driver_override_is_preserved_with_new_other_generation(self):
        result = self.invoke('macos', '--self-test', changes={'VK_ICD_FILENAMES': '/operator/icd'})
        self.assertEqual(result.returncode, 0, result.stderr)
        values = self.config_values(result)
        self.assertEqual(values['VK_ICD_FILENAMES'], '/operator/icd')
        self.assertTrue(Path(values['VK_DRIVER_FILES']).is_file())

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_linked_vk_directory_is_refused_before_writes(self):
        f = self.fixtures['macos']
        f['runtime'].mkdir()
        foreign = f['base'] / 'foreign'
        foreign.mkdir()
        (f['runtime'] / 'vk').symlink_to(foreign, target_is_directory=True)
        before = self.tree(f['base'])
        result = self.invoke('macos', '--self-test')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(before, self.tree(f['base']))

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_namespace_path_injection_is_refused(self):
        f = self.fixtures['macos']
        info = f['package'] / 'Contents/Info.plist'
        row = plistlib.loads(info.read_bytes())
        row['PhysicsSimRuntimeNamespace'] = '../outside'
        info.write_bytes(plistlib.dumps(row))
        before = self.tree(f['base'])
        result = self.invoke('macos', '--print-config', defaults=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('invalid package', result.stderr)
        self.assertEqual(before, self.tree(f['base']))

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_session_handoff_stdout_is_not_contaminated(self):
        f = self.fixtures['macos']
        session = f['base'] / 'session-stub.py'
        session.write_text('import json,sys;print(json.dumps({"args":sys.argv[1:]}))\n')
        result = self.invoke('macos', '--agent-mcp', '--fixture', changes={'PHYSICS_SIM_SESSION_TOOL': session})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), {'args': ['--mcp', '--fixture']})
        self.assertFalse(f['result'].exists())

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_icd_json_handles_quote_and_backslash_package_path(self):
        f = self.fixtures['macos']
        renamed = f['package'].with_name('kinetiC "quote" back\\slash.app')
        f['package'].rename(renamed)
        f['package'] = renamed
        f['launcher'] = renamed / 'Contents/MacOS/physics-sim-launcher'
        result = self.invoke('macos', '--self-test')
        self.assertEqual(result.returncode, 0, result.stderr)
        generated = Path(self.config_values(result)['VK_ICD_FILENAMES'])
        self.assertEqual(json.loads(generated.read_text())['ICD']['library_path'], str(renamed / 'Contents/Frameworks/libMoltenVK.dylib'))

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_icd_tool_failure_retains_candidate_without_launching_app(self):
        f = self.fixtures['macos']
        vk = f['runtime'] / 'vk'
        vk.mkdir(parents=True)
        (vk / 'MoltenVK_icd.json').write_bytes(b'previous')
        tool = f['base'] / 'plutil-failure'
        tool.write_text('#!/bin/sh\necho synthetic-plutil-failure >&2\nexit 19\n')
        tool.chmod(0o755)
        f['launcher'].write_text(f['launcher'].read_text().replace('/usr/bin/plutil', '"' + str(tool) + '"'))
        result = self.invoke('macos', '--fixture')
        self.assertEqual(result.returncode, 19)
        self.assertIn('synthetic-plutil-failure', result.stderr)
        self.assertEqual((vk / 'MoltenVK_icd.json').read_bytes(), b'previous')
        self.assertEqual(len(list(vk.glob('MoltenVK_icd.*'))), 2)
        self.assertFalse(f['result'].exists())

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_both_driver_overrides_leave_unused_linked_vk_path_untouched(self):
        f = self.fixtures['macos']
        f['runtime'].mkdir()
        foreign = f['base'] / 'foreign'
        foreign.mkdir()
        (f['runtime'] / 'vk').symlink_to(foreign, target_is_directory=True)
        result = self.invoke('macos', '--self-test', changes={'VK_ICD_FILENAMES': '/operator/icd', 'VK_DRIVER_FILES': '/operator/driver'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(list(foreign.iterdir()), [])

    @unittest.skipUnless(MAC_TOOLS, 'requires native macOS plist tools')
    def test_macos_previous_fixed_icd_link_is_not_followed_or_replaced(self):
        f = self.fixtures['macos']
        vk = f['runtime'] / 'vk'
        vk.mkdir(parents=True)
        foreign = f['base'] / 'foreign-icd'
        foreign.write_bytes(b'foreign')
        old = vk / 'MoltenVK_icd.json'
        old.symlink_to(foreign)
        before = foreign.stat().st_mtime_ns
        result = self.invoke('macos', '--self-test')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(old.is_symlink())
        self.assertEqual(foreign.read_bytes(), b'foreign')
        self.assertEqual(foreign.stat().st_mtime_ns, before)

    def test_linux_explicit_xdg_defaults_are_inspected_without_allocation(self):
        f = self.fixtures['linux']
        result = self.invoke('linux', '--print-config', defaults=True, changes={'XDG_DATA_HOME': f['base'] / 'xdg data', 'XDG_STATE_HOME': f['base'] / 'xdg state'})
        values = self.config(result)
        self.assertEqual(values['PHYSICS_SIM_RUNTIME_DIR'], str(f['base'] / 'xdg data/PhysicsSim/runtime'))
        self.assertEqual(values['LOG_FILE'], str(f['base'] / 'xdg state/PhysicsSim/logs/package_launcher.log'))
        self.assertFalse((f['base'] / 'xdg data').exists())
        self.assertFalse((f['base'] / 'xdg state').exists())


if __name__ == '__main__':
    unittest.main()
