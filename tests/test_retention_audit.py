"""Retention audit is bounded, read-only and never promotes metadata to deletion."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from retention_audit import audit,policy


class RetentionAudit(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();(self.repo/'config').mkdir()
        shutil.copy2(ROOT/'config/artifact_retention_policy.json',self.repo/'config/artifact_retention_policy.json')
        self.rules,_=policy(self.repo)

    def marker(self,path,kind):
        path.mkdir(parents=True,exist_ok=True)
        (path/'receipt.json').write_text(json.dumps({'artifact_class':kind,'status':'passed'}))

    def test_classes_and_mixed_build_remain_held_even_with_passed_receipt(self):
        root=self.repo/'build/profile';self.marker(root/'runs/proof','retained_contract_proof')
        (root/'binary').write_bytes(b'owned status not inferred')
        before={str(p):p.read_bytes() for p in root.rglob('*') if p.is_file()}
        row=audit(self.repo,root,self.rules)
        self.assertEqual(row['observed_classes'],['disposable_build','retained_contract_proof','retained_evidence'])
        self.assertEqual(row['regular_files'],2)
        self.assertFalse(row['eligible_for_pruning']);self.assertFalse(row['archive_coverage_verified'])
        self.assertFalse(row['terminal_ownership_verified']);self.assertTrue(row['inventory_complete'])
        self.assertEqual(before,{str(p):p.read_bytes() for p in root.rglob('*') if p.is_file()})
        self.assertIn('Mixed build',str(row['diagnostics']))

    def test_native_metadata_lock_and_pending_are_held_classes(self):
        root=self.repo/'build/jobs/proof';root.mkdir(parents=True)
        (root/'.physics-sim-job-metadata.lock').write_bytes(b'')
        (root/'.headless-sidecar-123-1.pending').write_bytes(b'{"v":')
        row=audit(self.repo,root,self.rules)
        self.assertEqual(row['observed_classes'],['disposable_build','operational_job','retained_evidence'])
        self.assertFalse(row['eligible_for_pruning']);self.assertEqual(row['status'],'held')
        self.assertEqual((root/'.headless-sidecar-123-1.pending').read_bytes(),b'{"v":')

    def test_scratch_and_environment_are_not_automatically_retired(self):
        for location,kind in (('tmp/tests/fixture','fixture_scratch'),('data/tools/profile','reference_environment')):
            root=self.repo/location;self.marker(root,kind)
            row=audit(self.repo,root,self.rules)
            self.assertIn(kind,row['observed_classes']);self.assertFalse(row['eligible_for_pruning'])

    def test_no_link_following_or_special_file_reads(self):
        root=self.repo/'tmp/audit';root.mkdir(parents=True)
        outside=self.repo/'source';outside.mkdir();(outside/'secret').write_bytes(b'excluded')
        (root/'link').symlink_to(outside);os.mkfifo(root/'pipe')
        row=audit(self.repo,root,self.rules)
        self.assertEqual(row['regular_files'],0);self.assertEqual(row['symlinks'],1);self.assertEqual(row['special_files'],1)
        with self.assertRaises(ValueError):audit(self.repo,root/'link',self.rules)
        with self.assertRaises(ValueError):audit(self.repo,outside,self.rules)

    def test_inventory_limit_and_bad_metadata_hold_without_unbounded_read(self):
        root=self.repo/'tmp/audit';root.mkdir(parents=True)
        for index in range(20):(root/str(index)).write_bytes(b'content')
        row=audit(self.repo,root,self.rules,max_entries=3)
        self.assertFalse(row['inventory_complete']);self.assertLessEqual(row['entries_observed'],3)
        (root/'receipt.json').write_text('{"artifact_class":"fixture_scratch","artifact_class":"release_artifact"}')
        row=audit(self.repo,root,self.rules)
        self.assertFalse(row['inventory_complete']);self.assertIn('Duplicate JSON',str(row['diagnostics']))
        with self.assertRaises(ValueError):audit(self.repo,root,self.rules,wall_cap=float('nan'))

    def test_absent_roots_are_not_created_and_bundle_presence_is_not_integrity(self):
        root=self.repo/'data/experiments/proof'
        row=audit(self.repo,root,self.rules);self.assertEqual(row['status'],'absent');self.assertFalse(root.exists())
        root.mkdir(parents=True);(root/'bundle_manifest.json').write_bytes(b'not a valid manifest')
        row=audit(self.repo,root,self.rules);self.assertEqual(row['observed_classes'],['retained_evidence'])
        self.assertFalse(row['integrity_verified']);self.assertFalse(row['eligible_for_pruning'])

    def test_actual_control_make_audit_does_not_load_build_fragments_or_write(self):
        for folder in ('scripts','make'):(self.repo/folder).mkdir()
        for name in ('retention_audit.py','cfd_evidence.py','check_clean_root.py'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        for name in ('config.mk','sources-tools.mk','rules-runtime.mk'):
            shutil.copy2(ROOT/'make'/name,self.repo/'make'/name)
        shutil.copy2(ROOT/'makefile',self.repo/'makefile')
        for name in ('VERSION','WORKER_VERSION'):(self.repo/name).write_text('test\n')
        before={str(p):p.read_bytes() for p in self.repo.rglob('*') if p.is_file()}
        env={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
        result=subprocess.run(['make','retention-audit'],cwd=self.repo,env=env,text=True,capture_output=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr)
        row=json.loads(result.stdout);self.assertEqual(row['mutations_performed'],[])
        self.assertFalse(row['pruning_authorized']);self.assertTrue(all(item['status']=='absent' for item in row['inventory']))
        self.assertEqual(before,{str(p):p.read_bytes() for p in self.repo.rglob('*') if p.is_file()})
        for name in ('build','tmp','data'):self.assertFalse((self.repo/name).exists())


if __name__=='__main__':unittest.main()
