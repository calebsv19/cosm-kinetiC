"""Recovery against actual native interrupted cache publication."""
import fcntl
import json
import os
from pathlib import Path
import sys
import unittest
from unittest import mock
import test_cache_publish_transaction as native
sys.path.insert(0,str(native.ROOT/'scripts'))
import cache_publication_recovery as recovery

class CacheRecovery(native.CacheTransaction):
    def interrupted(self,boundary=6):
        self.predecessors();env=os.environ.copy();env['CACHE_TEST_RENAME_DEATH']=str(boundary)
        self.assertEqual(self.invoke(env=env).returncode,77)
    def assert_original(self):
        self.assert_retained([p/'retained' for p in self.slots()])
        for p in self.targets()[4:]:self.assertEqual(p.read_bytes(),b'previous-manifest')
    def test_all_native_rename_boundaries_have_readonly_plan_and_verified_rollback(self):
        for boundary in range(1,15):
            with self.subTest(boundary=boundary):
                self.setUp();self.interrupted(boundary);pending=self.project/'physics_sim/.cache-publication.pending'
                before=pending.read_bytes();plan=recovery.plan(self.project);self.assertEqual(pending.read_bytes(),before)
                self.assertFalse(plan['mutation_performed']);self.assertFalse(plan['forward_promotion_allowed'])
                result=recovery.rollback(self.project,plan['plan_sha256']);self.assertEqual(result['status'],'rollback_content_verified')
                self.assert_original();self.assertFalse(pending.exists());self.assertTrue((self.attempts()[0]/'rolled-back').exists())
                self.assertEqual(self.invoke().returncode,0)
    def test_relative_publication_journal_is_recoverable_from_another_cwd(self):
        self.predecessors();env=os.environ.copy();env['CACHE_TEST_RENAME_DEATH']='6'
        import subprocess
        result=subprocess.run([str(self.binary),'project','project/physics_sim/runs/run-good','run-good','publish'],cwd=self.base,env=env,capture_output=True,timeout=5)
        self.assertEqual(result.returncode,77)
        plan=recovery.plan(self.project);self.assertTrue(all(Path(row['target']).is_absolute() for row in plan['slots']))
        recovery.rollback(self.project,plan['plan_sha256']);self.assert_original()
    def test_whole_plan_rejects_drift_after_an_earlier_slot_was_observed(self):
        self.interrupted();original=recovery.observe;calls=0
        def changed(selected,budget):
            nonlocal calls
            result=original(selected,budget);calls+=1
            if calls==8:(self.attempts()[0]/'prior-0'/'retained').write_bytes(b'changed')
            return result
        with mock.patch.object(recovery,'observe',changed):
            with self.assertRaises(ValueError):recovery.plan(self.project)
    def test_operator_cli_defaults_to_readonly_and_requires_matching_digest(self):
        self.interrupted();import subprocess
        command=[sys.executable,'-B',str(native.ROOT/'scripts/cache_publication_recovery.py'),'--project',str(self.project)]
        read=subprocess.run(command,capture_output=True,text=True,timeout=10);self.assertEqual(read.returncode,0,read.stdout)
        result=json.loads(read.stdout);self.assertFalse(result['mutation_performed'])
        missing=subprocess.run(command+['--rollback'],capture_output=True,text=True,timeout=10);self.assertEqual(missing.returncode,2)
        self.assertTrue((self.project/'physics_sim/.cache-publication.pending').exists())
        applied=subprocess.run(command+['--rollback','--expected-plan-sha256',result['plan_sha256']],capture_output=True,text=True,timeout=10)
        self.assertEqual(applied.returncode,0,applied.stdout);self.assertEqual(json.loads(applied.stdout)['status'],'rollback_content_verified');self.assert_original()
    def test_wrong_or_changed_plan_never_mutates(self):
        self.interrupted();plan=recovery.plan(self.project)
        with self.assertRaises(ValueError):recovery.rollback(self.project,'0'*64)
        attempt=self.attempts()[0];candidate=attempt/'new-3'/'scene_bundle.json';candidate.write_text('changed')
        with self.assertRaises(ValueError):recovery.rollback(self.project,plan['plan_sha256'])
        self.assertTrue((self.project/'physics_sim/.cache-publication.pending').exists())
    def test_changed_target_scope_and_duplicate_journal_are_held(self):
        self.interrupted();journal=self.attempts()[0]/'plan.json';original=journal.read_text();row=json.loads(original);row['targets'][-1]['path']=str(self.base/'outside')
        journal.write_text(json.dumps(row))
        with self.assertRaises(ValueError):recovery.plan(self.project)
        journal.write_text(original.replace('"schema":','"schema":"duplicate","schema":',1))
        with self.assertRaises(ValueError):recovery.plan(self.project)
    def test_busy_owner_symlink_hardlink_and_unknown_attempt_entry_hold(self):
        self.interrupted();lock=self.project/'physics_sim/.cache-publication.lock'
        with lock.open('rb') as f:
            fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
            with self.assertRaises(ValueError):recovery.plan(self.project)
        attempt=self.attempts()[0];unknown=attempt/'unknown';unknown.write_text('unclassified')
        with self.assertRaises(ValueError):recovery.plan(self.project)
        unknown.unlink();candidate=attempt/'new-3'/'scene_bundle.json';old=candidate.read_bytes();candidate.unlink();candidate.symlink_to(self.base/'absent')
        with self.assertRaises(ValueError):recovery.plan(self.project)
        candidate.unlink();candidate.write_bytes(old);os.link(candidate,attempt/'new-3'/'hardlinked')
        with self.assertRaises(ValueError):recovery.plan(self.project)
    def test_interrupted_rollback_can_be_replanned_and_resumed_without_deletion(self):
        self.interrupted(10);plan=recovery.plan(self.project);rename=recovery.os.rename;calls=0
        def failed(source,target):
            nonlocal calls
            calls+=1
            if calls==2:raise OSError('controlled rollback interruption')
            return rename(source,target)
        with mock.patch.object(recovery.os,'rename',failed):
            with self.assertRaises(OSError):recovery.rollback(self.project,plan['plan_sha256'])
        self.assertTrue((self.project/'physics_sim/.cache-publication.pending').exists())
        resumed=recovery.plan(self.project);recovery.rollback(self.project,resumed['plan_sha256']);self.assert_original()
    def test_staged_drift_after_first_slot_publish_holds_and_rolls_back(self):
        self.predecessors();env=os.environ.copy();env['CACHE_TEST_STAGE_RENAME_MUTATE']='1'
        self.assertEqual(self.invoke(env=env).returncode,2)
        attempt=self.attempts()[0]
        self.assertEqual((attempt/'prior-0/retained').read_bytes(),b'previous')
        self.assertFalse((attempt/'prior-1').exists())
        self.assertEqual((self.slots()[1]/'retained').read_bytes(),b'previous')
        self.assertEqual(self.invoke(mode='status').returncode,2)
        plan=recovery.plan(self.project);result=recovery.rollback(self.project,plan['plan_sha256'])
        self.assertEqual(result['status'],'rollback_content_verified');self.assert_original()
        self.assertTrue((attempt/'rollback-new-0').is_dir())
        self.assertTrue((attempt/'new-1').is_dir())

    def test_copy_failure_rolls_back_without_promoting_partial_candidates(self):
        self.predecessors();env=os.environ.copy();env['CACHE_TEST_COPY_FAILURE']='1';self.assertEqual(self.invoke(env=env).returncode,2)
        plan=recovery.plan(self.project);result=recovery.rollback(self.project,plan['plan_sha256'])
        self.assertTrue(result['new_output_retained']);self.assert_original();self.assertTrue((self.attempts()[0]/'new-0').is_dir())

if __name__=='__main__':unittest.main()
