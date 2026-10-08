"""Exact archived coverage is separate from ownership and pruning authority."""
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import test_restore_rehearsal as helpers
from cfd_evidence import seal_bundle
from restore_rehearsal import rehearse
from evidence_retirement_plan import plan, snapshot


class RetirementPlan(unittest.TestCase):
    def setUp(self):
        self.helper = helpers.Restore()
        self.helper.setUp()
        self.addCleanup(self.helper.doCleanups)
        self.base = self.helper.root
        self.repo = self.base/'repo'
        (self.repo/'config').mkdir(parents=True)
        shutil.copyfile(ROOT/'config/artifact_retention_policy.json', self.repo/'config/artifact_retention_policy.json')
        payload, raw_copy = self.helper.payload()
        self.copy = self.base/'copy-receipt/receipt.json'
        self.copy.parent.mkdir()
        shutil.copyfile(raw_copy, self.copy)
        seal_bundle(self.copy.parent)
        result = rehearse(payload, self.copy, self.base/'restored')
        self.restore = self.base/'restore-receipt/receipt.json'
        self.restore.parent.mkdir()
        self.restore.write_text(json.dumps(result))
        seal_bundle(self.restore.parent)
        self.bundle = self.repo/'data/experiments/proof'
        shutil.copytree(self.base/'fresh/data/experiments/proof', self.bundle)
        self.readback = self.base/'restored/fresh/data/experiments/proof'

    def run_plan(self):
        return plan(self.repo, self.bundle, self.copy, self.restore)

    def test_exact_coverage_remains_held_and_is_read_only(self):
        before = snapshot(self.repo)
        row = self.run_plan()
        self.assertEqual(row['status'], 'covered_snapshot_held')
        self.assertTrue(row['exact_file_coverage_verified'])
        self.assertFalse(row['terminal_ownership_verified'])
        self.assertFalse(row['pruning_authorized'])
        self.assertFalse(row['eligible_for_pruning'])
        self.assertEqual(row['mutations_performed'], [])
        self.assertEqual(snapshot(self.repo), before)
        self.assertIn('bundle_manifest.json', row['current_snapshot']['files'])

    def test_current_extra_changed_or_missing_files_are_held(self):
        for action in ('extra', 'changed', 'missing'):
            with self.subTest(action=action):
                original = (self.bundle/'input').read_bytes()
                if action == 'extra': (self.bundle/'extra').write_text('unknown')
                elif action == 'changed': (self.bundle/'input').write_text('changed')
                else: (self.bundle/'input').unlink()
                with self.assertRaises(ValueError): self.run_plan()
                (self.bundle/'input').write_bytes(original)
                if action == 'extra': (self.bundle/'extra').unlink()

    def test_changed_restore_and_extra_empty_directory_are_held(self):
        (self.readback/'input').write_text('changed')
        with self.assertRaises(ValueError): self.run_plan()
        (self.readback/'input').write_bytes((self.bundle/'input').read_bytes())
        (self.readback/'extra-empty').mkdir()
        with self.assertRaises(ValueError): self.run_plan()

    def test_unsealed_or_changed_receipts_are_held(self):
        (self.copy.parent/'bundle_manifest.json').unlink()
        with self.assertRaises((ValueError, FileNotFoundError)): self.run_plan()
        seal_bundle(self.copy.parent)
        self.restore.write_text('{}')
        with self.assertRaises(ValueError): self.run_plan()

    def test_resealed_mismatched_restore_binding_is_held(self):
        row = json.loads(self.restore.read_text())
        row['source_copy_receipt_sha256'] = '0'*64
        self.restore.write_text(json.dumps(row))
        (self.restore.parent/'bundle_manifest.json').unlink()
        seal_bundle(self.restore.parent)
        with self.assertRaisesRegex(ValueError, 'not bound'): self.run_plan()

    def test_uncovered_bundle_and_protected_selection_are_held(self):
        other = self.repo/'data/experiments/later'
        shutil.copytree(self.bundle, other)
        with self.assertRaisesRegex(ValueError, 'outside verified'): plan(self.repo, other, self.copy, self.restore)
        with self.assertRaises(ValueError): plan(self.repo, self.repo/'config', self.copy, self.restore)

    def test_links_special_files_and_enumeration_bounds_are_held(self):
        (self.bundle/'link').symlink_to(self.readback/'input')
        with self.assertRaises(ValueError): self.run_plan()
        (self.bundle/'link').unlink()
        os.link(self.bundle/'input', self.bundle/'hardlink')
        with self.assertRaises(ValueError): self.run_plan()
        (self.bundle/'hardlink').unlink()
        os.mkfifo(self.bundle/'pipe')
        with self.assertRaises(ValueError): self.run_plan()
        (self.bundle/'pipe').unlink()
        with self.assertRaises(ValueError): snapshot(self.bundle, max_entries=1)

    def test_excluded_service_lock_bytes_must_still_match_readback(self):
        (self.bundle/'service.lock').write_bytes(b'current')
        with self.assertRaisesRegex(ValueError, 'inventories differ'): self.run_plan()
        (self.readback/'service.lock').write_bytes(b'current')
        row = self.run_plan()
        self.assertIn('service.lock', row['current_snapshot']['files'])
        self.assertFalse(row['eligible_for_pruning'])


if __name__ == '__main__': unittest.main()
