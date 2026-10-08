"""Fail closed on dangerous retained roots and malformed or incomplete inventories."""
import json
import os
from pathlib import Path
import sys
import tempfile
import shutil
import subprocess
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import cfd_evidence as evidence


class Admission(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name).resolve()

    def bundle(self):
        bundle=self.repo/'data/experiments/bundle';bundle.mkdir(parents=True)
        (bundle/'artifact').write_bytes(b'retained');return bundle

    def test_experiment_roots_refuse_source_tools_and_configured_overlap(self):
        for name in ('src','include','scripts','tests','docs','make','config','third_party','.git','.codex','export','dist','data/tools','build','tmp'):
            with self.subTest(name=name), self.assertRaises(ValueError):evidence.experiment_root(self.repo,self.repo/name/'evidence')
        with patch.dict(os.environ,{'PHYSICS_SIM_BUILD_ROOT':str(self.repo/'profiles/generated')}):
            with self.assertRaises(ValueError):evidence.experiment_root(self.repo,self.repo/'profiles')
        with patch.dict(os.environ,{'PHYSICS_SIM_REFERENCE_TOOLS_ROOT':str(self.repo/'reference-env')}):
            with self.assertRaises(ValueError):evidence.experiment_root(self.repo,self.repo/'reference-env/runs')
        self.assertEqual(evidence.experiment_root(self.repo,self.repo/'data/experiments'),self.repo/'data/experiments')
        self.assertEqual(evidence.experiment_root(self.repo,self.repo/'retained'),self.repo/'retained')

    def test_active_native_cli_rejects_source_root_before_creating_run(self):
        scripts = self.repo/'scripts';scripts.mkdir()
        for name in ('run_cfd_native_box.py','cfd_run_support.py','cfd_evidence.py','check_clean_root.py'):
            shutil.copy2(ROOT/'scripts'/name,scripts/name)
        source = self.repo/'src';source.mkdir();(source/'sentinel.c').write_bytes(b'original source')
        environment = dict(os.environ)
        for name in ('PHYSICS_SIM_BUILD_ROOT','PHYSICS_SIM_TEST_ROOT','PHYSICS_SIM_EXPERIMENT_ROOT','PHYSICS_SIM_REFERENCE_TOOLS_ROOT'):
            environment.pop(name,None)
        result = subprocess.run([sys.executable,'-B','scripts/run_cfd_native_box.py',
            '--experiment-root','src','--name','refused','--grid','16','8','8','--length','4',
            '--lower-m','1.5','.5','.5','--upper-m','2.5','1.5','1.5'],
            cwd=self.repo,env=environment,capture_output=True,text=True,timeout=10)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('overlap protected storage',result.stderr)
        self.assertEqual(list(source.iterdir()),[source/'sentinel.c'])
        self.assertEqual((source/'sentinel.c').read_bytes(),b'original source')
        self.assertFalse((self.repo/'data/experiments').exists())

    def test_readback_cli_output_overrides_refuse_source_before_copying(self):
        scripts=self.repo/'scripts';scripts.mkdir()
        for name in ('check_cfd_native_cube_readback.py','check_cfd_native_pressure_trace_api.py',
                     'cfd_run_support.py','cfd_evidence.py','check_clean_root.py'):
            shutil.copy2(ROOT/'scripts'/name,scripts/name)
        # No numerical functions run in this containment test. A controlled
        # import keeps this guard test independent of the reference environment.
        (scripts/'numpy.py').write_text('# numerical dependency is unused before admission\n')
        source=self.repo/'src';source.mkdir();sentinel=source/'sentinel.c';sentinel.write_bytes(b'original')
        bundle=self.repo/'data/experiments/input';bundle.mkdir(parents=True)
        (bundle/'receipt.json').write_text(json.dumps({'status':'completed_native_cube_pressure_comparison','control':{}}))
        evidence.seal_bundle(bundle);before=evidence.verify_bundle(bundle)
        environment=dict(os.environ)
        for name in ('PHYSICS_SIM_BUILD_ROOT','PHYSICS_SIM_TEST_ROOT','PHYSICS_SIM_EXPERIMENT_ROOT','PHYSICS_SIM_REFERENCE_TOOLS_ROOT'):
            environment.pop(name,None)
        for name in ('check_cfd_native_cube_readback.py','check_cfd_native_pressure_trace_api.py'):
            result=subprocess.run([sys.executable,'-B',str(scripts/name),str(bundle/'receipt.json'),'--output','src/refused'],
                cwd=self.repo,env=environment,capture_output=True,text=True,timeout=10)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('overlap protected storage',result.stderr)
            self.assertFalse((source/'refused').exists());self.assertEqual(sentinel.read_bytes(),b'original')
            self.assertEqual(evidence.verify_bundle(bundle),before)

    def test_symlink_roots_components_and_artifacts_refused(self):
        bundle=self.bundle();alias=self.repo/'alias';alias.symlink_to(bundle,target_is_directory=True)
        for path in (alias,alias/'nested'):
            with self.assertRaises(ValueError):evidence.experiment_root(self.repo,path)
        with self.assertRaises(ValueError):evidence.seal_bundle(alias)
        (bundle/'link').symlink_to(bundle/'artifact')
        with self.assertRaises(ValueError):evidence.seal_bundle(bundle)
        self.assertFalse((bundle/'bundle_manifest.json').exists())

    def test_special_files_refused_even_as_lock_or_manifest(self):
        bundle=self.bundle()
        for name in ('unexpected.pipe','service.lock','bundle_manifest.json'):
            path=bundle/name;os.mkfifo(path)
            with self.assertRaisesRegex(ValueError,'non-regular'):evidence.seal_bundle(bundle)
            with self.assertRaisesRegex(ValueError,'non-regular'):evidence.verify_bundle(bundle)
            with self.assertRaisesRegex(ValueError,'regular file'):evidence.sha(path)
            path.unlink()
        evidence.seal_bundle(bundle);os.mkfifo(bundle/'after-seal.pipe')
        with self.assertRaises(ValueError):evidence.verify_bundle(bundle)

    def test_empty_bundle_does_not_publish_manifest(self):
        self.assertRaises(ValueError,evidence.seal_bundle,self.repo)
        self.assertFalse((self.repo/'bundle_manifest.json').exists())

    def test_duplicate_nonfinite_and_invalid_schema_metadata_refused(self):
        bundle=self.bundle();evidence.seal_bundle(bundle);path=bundle/'bundle_manifest.json'
        original=path.read_text();row=json.loads(original)
        bad=[original.replace('"schema":', '"schema": "duplicate", "schema":',1),original.replace('"backup_verified": false','"backup_verified": NaN'), '[]', 'null']
        for value in (True,-1,1.5,'8',None):
            changed=json.loads(original);changed['files']['artifact']['bytes']=value;bad.append(json.dumps(changed))
        for value in (None,'bad','0'*63,1):
            changed=json.loads(original);changed['files']['artifact']['sha256']=value;bad.append(json.dumps(changed))
        for value in (None,0,'false'):
            changed=json.loads(original);changed['backup_verified']=value;bad.append(json.dumps(changed))
        for content in bad:
            with self.subTest(content=content[:50]):
                path.write_text(content)
                with self.assertRaises(ValueError):evidence.verify_bundle(bundle)
        path.write_text(original);evidence.verify_bundle(bundle)

    def test_oversized_manifest_is_refused_before_content_read(self):
        bundle=self.bundle();evidence.seal_bundle(bundle)
        with (bundle/'bundle_manifest.json').open('r+b') as stream:
            stream.truncate(64*1024*1024+1)
        with self.assertRaisesRegex(ValueError,'oversized JSON'):
            evidence.verify_bundle(bundle)
        self.assertEqual((bundle/'artifact').read_bytes(),b'retained')

    def test_alias_artifact_names_are_not_canonical_paths(self):
        bundle=self.bundle();evidence.seal_bundle(bundle)
        for reference in ('./artifact','artifact/../artifact','artifact/','', '.', '/artifact', '../artifact'):
            with self.assertRaises(ValueError):evidence.resolve_artifact(bundle,reference)
        self.assertEqual(evidence.resolve_artifact(bundle,'artifact'),bundle/'artifact')

    def test_mutation_after_earlier_digest_is_detected_at_final_inventory_check(self):
        bundle=self.bundle();(bundle/'later').write_bytes(b'other');evidence.seal_bundle(bundle)
        original=evidence.file_fingerprint
        def observed(path):
            result=original(path)
            if path.name=='later':(bundle/'artifact').write_bytes(b'changed!')
            return result
        with patch.object(evidence,'file_fingerprint',side_effect=observed):
            with self.assertRaisesRegex(ValueError,'changed during readback'):evidence.verify_bundle(bundle)

    def test_readback_cli_reports_a_hold_without_traceback(self):
        bundle=self.bundle();os.mkfifo(bundle/'bundle_manifest.json')
        result=subprocess.run([sys.executable,'-B',str(ROOT/'scripts/cfd_evidence.py'),str(bundle)],
                              capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,2)
        self.assertIn('Retained readback held:',result.stderr)
        self.assertNotIn('Traceback',result.stderr)
        self.assertEqual((bundle/'artifact').read_bytes(),b'retained')

    def test_resealing_preserves_the_original_manifest(self):
        bundle=self.bundle();evidence.seal_bundle(bundle);before=(bundle/'bundle_manifest.json').read_bytes()
        with self.assertRaisesRegex(ValueError,'already sealed'):evidence.seal_bundle(bundle)
        self.assertEqual((bundle/'bundle_manifest.json').read_bytes(),before)


if __name__ == '__main__':unittest.main()
