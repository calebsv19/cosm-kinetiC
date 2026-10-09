"""Review-only bound bundler admission; canonical source is unchanged."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import macos_bundle
import package_paths
from package_outputs import plan, declare


class BoundBundler(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name).resolve()
        self.repo = self.base / 'source/physics_sim'
        self.repo.mkdir(parents=True)
        self.data = self.base / 'data'
        self.data.mkdir()
        patched = patch.object(package_paths, 'DATA_ROOT', self.data)
        patched.start()
        self.addCleanup(patched.stop)
        selected = 'physics_sim/build/release-authenticated/raor_' + 'a' * 64
        target = 'rapt_' + package_paths.digest({'package_target': 'release-local-artifact'})
        self.root = self.data / selected / 'targets' / target
        contract = {
            'schema_version': 'production-registry/release-authorization-precommit-preparation-contract/v1',
            'program': 'physics_sim',
            'source_data_roots': {'source_workspace_root': str(self.repo.parent), 'data_workspace_root': str(self.data)},
            'owner_adapter_binding': {'repository_path': 'physics_sim', 'package_targets': ['release-local-artifact']},
            'output_root_binding': {'selected_root': selected, 'candidate_scope_id': 'raor_' + 'a' * 64}}
        store = self.data / package_paths.CONTRACTS
        store.mkdir(parents=True)
        self.contract_file = store / (package_paths.digest(contract) + '.json')
        self.contract_file.write_text(json.dumps(contract))
        self.app = self.root / 'kinetiC.app'
        validate = lambda: plan(self.repo, self.root, [self.app], [])
        declare(validate(), validate)
        self.binary = self.app / 'Contents/MacOS/physics-sim-bin'
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(b'preserved')
        self.frameworks = self.app / 'Contents/Frameworks'
        self.frameworks.mkdir()
        self.receipt = next((self.root / '.package-reservations').glob('*.json'))

    def admit(self):
        return macos_bundle.admission(self.repo, self.binary, self.frameworks)

    def test_exact_bound_target_and_reservation_admitted(self):
        result = self.admit()
        self.assertEqual(result[2], self.receipt)
        self.assertEqual(self.binary.read_bytes(), b'preserved')

    def test_bound_target_runs_existing_owned_engine_successfully(self):
        for relative in ('scripts/macos_bundle.py', 'scripts/macos_bundle_engine.sh',
                         'tools/packaging/macos/bundle-dylibs.sh'):
            destination = self.repo / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, destination)
        otool = self.repo / 'otool-stub'
        otool.write_text('#!/bin/sh\nprintf "binary:\\n"\n')
        otool.chmod(0o755)
        install = self.repo / 'install-stub'
        install.write_text('#!/bin/sh\nexit 0\n')
        install.chmod(0o755)
        engine = self.repo / 'scripts/macos_bundle_engine.sh'
        engine.write_text(engine.read_text().replace('OTOOL_BIN="/usr/bin/otool"',
            'OTOOL_BIN="' + str(otool) + '"').replace('INSTALL_NAME_TOOL_BIN="/usr/bin/install_name_tool"',
            'INSTALL_NAME_TOOL_BIN="' + str(install) + '"'))
        with patch.dict(os.environ, {'PACKAGE_DEP_SEARCH_ROOTS': str(self.repo / 'dependencies')}, clear=True):
            attempt = macos_bundle.run(self.repo, self.binary, self.frameworks)
        receipt = json.loads((attempt / 'receipt.json').read_text())
        self.assertEqual(receipt['status'], 'passed')
        self.assertEqual(receipt['reservation'], str(self.receipt))
        self.assertEqual(self.binary.read_bytes(), b'preserved')

    def test_missing_contract_and_unreserved_app_refused(self):
        contents = self.contract_file.read_bytes()
        self.contract_file.unlink()
        with self.assertRaises(ValueError):
            self.admit()
        self.contract_file.write_bytes(contents)
        self.receipt.unlink()
        with self.assertRaisesRegex(ValueError, 'reservation'):
            self.admit()

    def test_tampered_output_authority_attempt_and_schema_refused(self):
        row = json.loads(self.receipt.read_text())
        for key, value in [('output', str(self.root.parent / 'escape.app')),
                           ('release_authority_granted', True), ('root', str(self.root.parent)),
                           ('attempt_id', 'invalid'), ('state', 'transaction_reserved')]:
            with self.subTest(key=key):
                changed = dict(row, **{key: value})
                self.receipt.write_text(json.dumps(changed))
                with self.assertRaises(ValueError):
                    self.admit()
        self.assertEqual(self.binary.read_bytes(), b'preserved')

    def test_symlinked_reservation_and_frameworks_refused(self):
        contents = self.receipt.read_bytes()
        outside = self.base / 'receipt.json'
        outside.write_bytes(contents)
        self.receipt.unlink()
        self.receipt.symlink_to(outside)
        with self.assertRaises(ValueError):
            self.admit()
        self.receipt.unlink()
        self.receipt.write_bytes(contents)
        (self.frameworks / 'linked').symlink_to(self.binary)
        with self.assertRaises(ValueError):
            self.admit()


if __name__ == '__main__':
    unittest.main()
