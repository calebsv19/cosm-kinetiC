"""Status distinguishes cleanup holds and exact backup coverage without writes."""
import json
import fcntl
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
from physics_status import status


class Status(unittest.TestCase):
    def test_fresh_selected_root_does_not_select_legacy_root_executables(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();legacy=repo/'physics_sim';legacy.write_bytes(b'legacy unregistered')
            row=status(repo,Path('build/fresh'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
            self.assertEqual(row['cleanup']['status'],'plan_available')
            selected=status(repo,Path('build/fresh'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'),[legacy])
            self.assertEqual(selected['cleanup']['status'],'held')
            self.assertEqual(legacy.read_bytes(),b'legacy unregistered')

    def test_readonly_missing_roots_are_not_created(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary).resolve()
            row = status(repo, Path('build'), Path('tmp/tests'), Path('data/experiments'), Path('data/tools'))
            self.assertEqual(list(repo.iterdir()), [])
            self.assertFalse(row['installed_or_published_state_verified'])
            self.assertFalse(row['backup_covers_all_current_evidence'])

    def test_status_distinguishes_parent_conflict_from_shared_sibling_intention(self):
        from build_owner import lock_path
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();(repo/'tmp/locks').mkdir(parents=True)
            with lock_path(repo,repo/'build').open('w') as parent:
                fcntl.flock(parent,fcntl.LOCK_EX|fcntl.LOCK_NB)
                row=status(repo,Path('build/a'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
                self.assertEqual(row['ownership']['hierarchy_admission'][0]['state'],'held')
                self.assertEqual(row['ownership']['selected_root']['state'],'absent')
                fcntl.flock(parent,fcntl.LOCK_SH)
                row=status(repo,Path('build/a'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
                self.assertEqual(row['ownership']['hierarchy_admission'][0]['state'],'available')

    def test_status_reports_live_lock_not_file_existence(self):
        from build_owner import lock_path
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();locks=repo/'tmp/locks';locks.mkdir(parents=True)
            path=lock_path(repo,repo/'build/selected')
            with path.open('w') as lock:
                fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
                row=status(repo,Path('build/selected'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
                self.assertEqual(row['ownership']['selected_root']['state'],'held')
            row=status(repo,Path('build/selected'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
            self.assertEqual(row['ownership']['selected_root']['state'],'available')

    def test_executable_status_uses_selected_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve()
            selected=repo/'build/selected/bin/physics_sim_headless'
            selected.parent.mkdir(parents=True);selected.write_bytes(b'selected')
            (repo/'physics_sim_headless').write_bytes(b'legacy')
            row=status(repo,Path('build/selected'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
            self.assertEqual(row['executables']['physics_sim_headless']['path'],str(selected))
            self.assertTrue(row['executables']['physics_sim_headless']['exists'])

    def test_bad_metadata_keeps_other_results_and_bytes_unchanged(self):
        invalid = (b'{', b'[]', b'null', b'\xff', b'{"digest":NaN}', b'x'*(1024*1024+1), b'['*2000+b'0'+b']'*2000, b'{"status":"x","status":"y"}')
        for content in invalid:
            with self.subTest(content=content[:30]), tempfile.TemporaryDirectory() as temporary:
                repo=Path(temporary).resolve()
                active=repo/'build/selected/.configuration/active.json'
                active.parent.mkdir(parents=True);active.write_bytes(content)
                receipt=repo/'data/experiments/lifecycle-validation/bad/receipt.json'
                receipt.parent.mkdir(parents=True);receipt.write_bytes(content)
                row=status(repo,Path('build/selected'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
                self.assertIsNone(row['build_configuration'])
                self.assertEqual(row['backup_receipts'],[])
                self.assertEqual({x['area'] for x in row['diagnostics']},{'build_configuration','backup_receipt'})
                self.assertIn('ownership',row);self.assertIn('cleanup',row)
                self.assertEqual(active.read_bytes(),content);self.assertEqual(receipt.read_bytes(),content)

    def test_field_validation_and_nonregular_metadata_do_not_claim_readiness(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();active=repo/'build/.configuration/active.json'
            active.parent.mkdir(parents=True)
            receipt=repo/'data/experiments/lifecycle-validation/bad/receipt.json'
            receipt.parent.mkdir(parents=True)
            for config,backup in (({'digest':'short','generation':1},{'status':'verified_independent_cold_archive_copy'}),
                                 ({'digest':'a'*64,'generation':True},{'status':'verified_independent_cold_archive_copy','archive_destination':'cold','coverage':'earlier','remote_unpack_or_retrieval_rehearsal_performed':'yes'})):
                active.write_text(json.dumps(config));receipt.write_text(json.dumps(backup))
                row=status(repo,Path('build'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
                self.assertIsNone(row['build_configuration']);self.assertEqual(row['backup_receipts'],[])
                self.assertEqual(len(row['diagnostics']),2)
            active.unlink();os.mkfifo(active)
            receipt.unlink();receipt.symlink_to(active)
            row=status(repo,Path('build'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
            self.assertEqual(len(row['diagnostics']),2)
            active.unlink();active.mkdir()
            row=status(repo,Path('build'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
            self.assertIsNone(row['build_configuration'])

    def test_git_failure_and_corrupt_version_are_diagnostics(self):
        for error in (FileNotFoundError('git unavailable'),subprocess.TimeoutExpired('git',15)):
            with self.subTest(error=error), tempfile.TemporaryDirectory() as temporary:
                repo=Path(temporary).resolve();(repo/'VERSION').write_bytes(b'\xff')
                with patch('physics_status.subprocess.run',side_effect=error):
                    row=status(repo,Path('build'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
                self.assertFalse(row['source_status']['git_available'])
                self.assertIsNone(row['source_versions']['program'])
                self.assertEqual(row['diagnostics'][0]['area'],'source_version')
                self.assertIn('reference_python',row)

    def test_retained_build_is_held_and_prior_backup_stays_scoped(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary).resolve()
            (repo/'build').mkdir(); (repo/'build/result.json').write_text('{"artifact_sha256":{}}')
            receipt=repo/'data/experiments/lifecycle-validation/backup/receipt.json'
            receipt.parent.mkdir(parents=True)
            receipt.write_text(json.dumps({'status':'verified_independent_cold_archive_copy','archive_destination':'cold/batch','coverage':'only earlier snapshot'}))
            row=status(repo, Path('build'), Path('tmp/tests'), Path('data/experiments'), Path('data/tools'))
            self.assertEqual(row['cleanup']['status'],'held')
            self.assertEqual(row['backup_receipts'][0]['coverage'],'only earlier snapshot')
            self.assertFalse(row['backup_covers_all_current_evidence'])


    def test_restore_rehearsal_is_bound_to_exact_sealed_copy_receipt_and_payload(self):
        from cfd_evidence import seal_bundle,sha
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();root=repo/'data/experiments/lifecycle-validation'
            copied=root/'copy';copied.mkdir(parents=True)
            payload={'snapshot.tar.gz':'a'*64}
            (copied/'receipt.json').write_text(json.dumps({'status':'verified_independent_cold_archive_copy',
                'archive_destination':'cold/exact','coverage':'prepared payload','payload_checksums':payload}))
            seal_bundle(copied)
            restored=root/'restore';restored.mkdir()
            receipt=restored/'receipt.json'
            receipt.write_text(json.dumps({'status':'verified_independent_archive_retrieval_and_restore',
                'archive_destination':'cold/exact','source_copy_receipt_sha256':sha(copied/'receipt.json'),
                'payload_checksums':payload,'historical_manifest_matched':True}))
            seal_bundle(restored)
            row=status(repo,Path('build'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
            self.assertTrue(row['backup_receipts'][0]['prepared_payload_retrieval_rehearsal_verified'])
            self.assertFalse(row['backup_covers_all_current_evidence'])
            receipt.write_text(receipt.read_text().replace('a'*64,'b'*64))
            row=status(repo,Path('build'),Path('tmp/tests'),Path('data/experiments'),Path('data/tools'))
            self.assertFalse(row['backup_receipts'][0]['prepared_payload_retrieval_rehearsal_verified'])
            self.assertEqual(row['archive_restore_rehearsals'],[])


if __name__=='__main__':unittest.main()
