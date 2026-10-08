"""Behavioral cleanup containment and all-or-nothing plan acceptance."""
import json
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIXTURE_ENV = {k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES')}
sys.path.insert(0, str(ROOT/'scripts'))
from clean_outputs import plan, apply
from build_outputs import record, receipt_path


class CleanOutputs(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name)
        self.build = self.repo/'build/proof'
        self.build.mkdir(parents=True)
        (self.build/'object.o').write_bytes(b'compiled')
        record(self.repo, self.build/'object.o', 'compiler')
        self.protected = [self.repo/n for n in ('data', 'src', 'tmp/tests')]

    def validate(self, outputs=()):
        return plan(self.repo, self.build, self.protected, outputs)

    def test_unknown_regular_file_and_empty_directory_hold_entire_cleanup(self):
        path = self.build/'notes.txt'
        path.write_bytes(b'valuable unclassified content')
        with self.assertRaisesRegex(ValueError, 'Unknown disposable output'):
            self.validate()
        self.assertEqual(path.read_bytes(), b'valuable unclassified content')
        self.assertTrue((self.build/'object.o').exists())
        path.unlink()
        path.mkdir()
        with self.assertRaisesRegex(ValueError, 'Unknown build directory'):
            self.validate()

    def test_owned_content_mutation_with_restored_mtime_is_held(self):
        obj = self.build/'object.o'
        planned = self.validate()
        info = obj.stat()
        obj.write_bytes(b'altered!')
        os.utime(obj, ns=(info.st_atime_ns, info.st_mtime_ns))
        with self.assertRaisesRegex(ValueError, 'Changed disposable output'):
            apply(planned, self.validate)
        self.assertTrue(obj.exists())

    def test_receipt_symlink_special_file_and_malformed_receipt_are_held(self):
        obj = self.build/'object.o'
        receipt = receipt_path(self.repo.resolve(), obj.resolve())
        original = receipt.read_bytes()
        receipt.unlink()
        receipt.symlink_to(obj)
        with self.assertRaises(ValueError): self.validate()
        receipt.unlink(); os.mkfifo(receipt)
        with self.assertRaises(ValueError): self.validate()
        receipt.unlink(); receipt.write_bytes(b'{')
        with self.assertRaises(ValueError): self.validate()
        receipt.write_bytes(original)
        self.assertEqual(len(self.validate()['owned_files']), 1)

    def test_nested_output_mutation_after_plan_is_not_hidden_by_root_identity(self):
        directory = self.build/'nested'
        directory.mkdir()
        output = directory/'child.o'
        output.write_bytes(b'compiled')
        record(self.repo, output, 'compiler')
        planned = self.validate()
        output.write_bytes(b'modified')
        with self.assertRaises(ValueError): apply(planned, self.validate)
        self.assertTrue((self.build/'object.o').exists())

    def test_registration_refuses_source_and_evidence_namespaces(self):
        for relative in ('src/probe.c', 'data/experiments/value', 'notes.txt'):
            path = self.repo/relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'preserved')
            with self.assertRaisesRegex(ValueError, 'namespace'):
                record(self.repo, path, 'compiler')
            self.assertEqual(path.read_bytes(), b'preserved')

    def test_retained_compiler_and_semantic_classes_are_held(self):
        path = self.build/'result.json'
        for kind in ('semantic_proof', 'retained_compiler_output', 'reference_environment'):
            path.write_text(json.dumps({'artifact_class': kind, 'status': 'failed'}))
            with self.assertRaisesRegex(ValueError, 'retained receipt'):
                self.validate()
            self.assertTrue((self.build/'object.o').exists())

    def test_owned_operational_and_session_receipts_override_disposable_claim(self):
        path=self.build/'attempt-result.json'
        for kind in ('operational_job','fixture_session','retained_evidence','fixture_visual_output'):
            path.write_text(json.dumps({'artifact_class':kind,'status':'completed','terminal_processes_verified':True}))
            record(self.repo,path,'compiler')
            with self.assertRaisesRegex(ValueError,'retained receipt'):self.validate()
            self.assertTrue(path.exists());self.assertTrue((self.build/'object.o').exists())

    def test_native_headless_owner_marker_holds_even_with_disposable_claim(self):
        path=self.build/'.physics-sim-headless-owner';path.write_bytes(b'native run evidence')
        record(self.repo,path,'compiler')
        with self.assertRaisesRegex(ValueError,'Retained evidence'):self.validate()
        self.assertTrue(path.exists());self.assertTrue((self.build/'object.o').exists())

    def test_metadata_lock_and_pending_stage_override_disposable_claim(self):
        for name in ('.physics-sim-job-metadata.lock','.physics-sim-job-operation.lock','.headless-sidecar-123-1.pending'):
            path=self.build/name;path.write_bytes(b'')
            record(self.repo,path,'compiler')
            with self.assertRaisesRegex(ValueError,'Retained evidence'):self.validate()
            self.assertTrue(path.exists());self.assertTrue((self.build/'object.o').exists())
            path.unlink()

    def test_every_output_is_checked_before_any_deletion(self):
        sentinel = self.repo/'data/experiments/field'
        sentinel.parent.mkdir(parents=True)
        sentinel.write_bytes(b'retained')
        for output in (sentinel.parent, self.repo/'src', self.repo.parent/'outside', self.repo/'unknown'):
            with self.assertRaises(ValueError): self.validate([output])
            self.assertTrue((self.build/'object.o').exists())
            self.assertEqual(sentinel.read_bytes(), b'retained')

    def test_named_and_renamed_receipts_are_both_retained(self):
        for name in ('receipt.json', 'accepted-result.json', 'nested.json'):
            path = self.build/name
            payload={'artifact_sha256': {'field': 'digest'}}
            path.write_text(json.dumps({'wrapped': [payload]} if name == 'nested.json' else payload))
            with self.assertRaises(ValueError): self.validate()
            path.unlink()

    def test_packages_jobs_and_operational_receipts_are_not_normal_clean(self):
        for name in ('release', 'release-authenticated', 'agent_runs', 'receipts'):
            directory = self.build/name
            directory.mkdir()
            (directory/'sentinel').write_bytes(b'retained owner')
            with self.assertRaises(ValueError): self.validate()
            with self.assertRaises(ValueError): plan(self.repo, directory, self.protected, [])
            self.assertEqual((directory/'sentinel').read_bytes(), b'retained owner')
            shutil.rmtree(directory)  # Only this test-created fixture.

    def test_symlink_components_and_inner_links_are_refused(self):
        outside = self.repo/'outside'
        outside.mkdir()
        (self.build/'alias').symlink_to(outside, target_is_directory=True)
        with self.assertRaises(ValueError): self.validate()
        (self.build/'alias').unlink()
        (self.repo/'physics_sim').symlink_to(self.repo/'missing')
        with self.assertRaises(ValueError): self.validate([self.repo/'physics_sim'])

    def test_nonregular_files_are_held_without_blocking_or_deleting(self):
        for name in ('unexpected.pipe','unexpected.json'):
            path=self.build/name;os.mkfifo(path)
            with self.assertRaisesRegex(ValueError,'non-regular'):self.validate()
            self.assertTrue(path.exists());self.assertEqual((self.build/'object.o').read_bytes(),b'compiled')
            path.unlink()
        reservation=self.repo/'build/.package-reservations/attempt.json'
        reservation.parent.mkdir();os.mkfifo(reservation)
        with self.assertRaisesRegex(ValueError,'reservation'):self.validate()
        self.assertTrue(reservation.exists())

    def test_changed_plan_never_deletes_other_targets(self):
        worker = self.repo/'physics_sim_session_worker'
        worker.write_bytes(b'worker')
        record(self.repo, worker, 'compiler')
        validate = lambda: self.validate([worker])
        planned = validate()
        worker.write_bytes(b'changed worker')
        with self.assertRaises(ValueError): apply(planned, validate)
        self.assertTrue(self.build.exists())
        self.assertTrue(worker.exists())

    def test_clean_removes_declared_worker_and_preserves_external_evidence(self):
        worker = self.repo/'physics_sim_session_worker'
        worker.write_bytes(b'worker')
        record(self.repo, worker, 'compiler')
        evidence = self.repo/'data/experiments/proof.json'
        evidence.parent.mkdir(parents=True)
        evidence.write_text('retained')
        validate = lambda: self.validate([worker])
        result = apply(validate(), validate)
        self.assertEqual(result['status'], 'cleaned')
        self.assertFalse(self.build.exists())
        self.assertFalse(worker.exists())
        self.assertEqual(evidence.read_text(), 'retained')

    def test_top_level_control_does_not_load_build_dependencies(self):
        for sub in ('make', 'scripts'): (self.repo/sub).mkdir()
        for name in ('config.mk', 'rules-runtime.mk', 'sources-tools.mk'):
            shutil.copy2(ROOT/'make'/name, self.repo/'make'/name)
        for name in ('check_clean_root.py', 'clean_outputs.py', 'build_outputs.py'):
            shutil.copy2(ROOT/'scripts'/name, self.repo/'scripts'/name)
        for name in ('VERSION', 'WORKER_VERSION'): (self.repo/name).write_text('test\n')
        shutil.copy2(ROOT/'makefile', self.repo/'makefile')
        # No compiler/flags/shared/object/package Make fragments are available.
        result = subprocess.run(['make', 'clean-plan', 'BUILD_DIR=build/proof', 'PKG_CONFIG=false'],
                                cwd=self.repo, env=FIXTURE_ENV, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertTrue((self.build/'object.o').exists())

    def test_actual_make_override_refuses_and_plan_is_nonmutating(self):
        for sub in ('make', 'scripts'): (self.repo/sub).mkdir()
        for name in ('config.mk', 'rules-runtime.mk'): shutil.copy2(ROOT/'make'/name, self.repo/'make'/name)
        for name in ('check_clean_root.py', 'clean_outputs.py', 'build_outputs.py'): shutil.copy2(ROOT/'scripts'/name, self.repo/'scripts'/name)
        for name in ('VERSION', 'WORKER_VERSION'): (self.repo/name).write_text('test\n')
        (self.repo/'Makefile').write_text('include make/config.mk\ninclude make/rules-runtime.mk\n')
        for arguments in (['clean-plan'], ['TARGET=data/experiments', 'clean']):
            result = subprocess.run(['make', 'BUILD_DIR=build/proof', *arguments], cwd=self.repo,env=FIXTURE_ENV,
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0 if arguments == ['clean-plan'] else 2, result.stdout+result.stderr)
            self.assertTrue((self.build/'object.o').exists())
        legacy = self.repo/'physics_sim_session_worker'
        legacy.write_bytes(b'legacy unselected worker')
        worker = self.build/'bin/physics_sim_session_worker'
        worker.parent.mkdir(); worker.write_bytes(b'compiled selected worker')
        record(self.repo, worker, 'compiler')
        result = subprocess.run(['make', 'BUILD_DIR=build/proof', 'clean'], cwd=self.repo,
                                env=FIXTURE_ENV, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertFalse(worker.exists())
        self.assertEqual(legacy.read_bytes(), b'legacy unselected worker')


if __name__ == '__main__': unittest.main()
