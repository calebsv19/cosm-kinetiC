"""Isolated build -> native run -> retain -> relocate -> clean -> rebuild proof.

Uses the actual cleanup recipe and native solver, never the retained build tree.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIXTURE_ENV = {k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES')}
sys.path.insert(0, str(ROOT/'scripts'))
from cfd_evidence import seal_bundle, verify_bundle, resolve_artifact, experiment_root, sha
from check_clean_root import check


class Lifecycle(unittest.TestCase):
    def test_native_bundle_survives_actual_make_clean_and_rebuild(self):
        with tempfile.TemporaryDirectory(prefix='physics-clean-acceptance-') as temporary:
            fixture = Path(temporary)
            (fixture/'make').mkdir()
            (fixture/'scripts').mkdir()
            for name in ('VERSION', 'WORKER_VERSION'):
                shutil.copy2(ROOT/name, fixture/name)
            for name in ('config.mk', 'rules-runtime.mk'):
                shutil.copy2(ROOT/'make'/name, fixture/'make'/name)
            for name in ('check_clean_root.py', 'clean_outputs.py', 'build_outputs.py', 'atomic_output.py', 'build_owner.py'):
                shutil.copy2(ROOT/'scripts'/name, fixture/'scripts'/name)
            sources = ['tests/cfd_obstacle3d_box_contract_test.c', 'src/app/cfd_obstacle3d.c',
                       'src/app/cfd_obstacle3d_box.c', 'src/app/cfd_obstacle3d_pressure_trace.c',
                       'src/app/cfd_obstacle3d_reconstruction.c', 'src/app/cfd_obstacle3d_mixed.c',
                       'src/app/cfd_cartesian3d.c', 'src/app/cfd_sparse_mg.c', 'src/app/cfd_memory.c']
            # Copy only native dependencies; no historical build directory or archive.
            shutil.copytree(ROOT/'include/app', fixture/'include/app')
            for name in sources:
                destination = fixture/name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT/name, destination)
            makefile = ('include make/config.mk\ninclude make/rules-runtime.mk\n'
                        'proof:\n\tmkdir -p "$(BUILD_DIR)"\n'
                        '\tpython3 -B scripts/atomic_output.py -- clang -std=c11 -O2 -Wall -Wextra -Werror -DCFD_MIXED3D_VERIFY -Iinclude '
                        + ' '.join(sources)+' -lm -o "$(BUILD_DIR)/contract"\n'
                        '\tmkdir -p tmp\n\t"$(BUILD_DIR)/contract" > tmp/results.jsonl\n')
            (fixture/'Makefile').write_text(makefile)
            def make(*arguments):
                result = subprocess.run(['make', *arguments], cwd=fixture, env=FIXTURE_ENV, check=False,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=180)
                if result.returncode:
                    self.fail(result.stdout)
                return result
            make('proof')
            retained = fixture/'data/experiments/run-original'
            retained.mkdir(parents=True)
            shutil.copy2(fixture/'tmp/results.jsonl', retained/'results.jsonl')
            (retained/'inputs.json').write_text(json.dumps({'source_sha256': {name: sha(fixture/name) for name in sources}, 'fixed_seed': None}))
            seal_bundle(retained)
            relocated = fixture/'data/experiments/run-relocated'
            shutil.copytree(retained, relocated)
            before = verify_bundle(relocated)
            make('clean')
            self.assertFalse((fixture/'build').exists())
            self.assertEqual(verify_bundle(retained), before)
            self.assertEqual(verify_bundle(relocated), before)
            make('proof')
            self.assertEqual(verify_bundle(relocated), before)
            self.assertEqual((fixture/'tmp/results.jsonl').read_bytes(), (retained/'results.jsonl').read_bytes())

    def test_tamper_missing_extra_and_escape_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory/'field.json').write_text('{"path":"field.json"}')
            seal_bundle(directory)
            verify_bundle(directory)
            for name in ('../outside', '/private/tmp/outside'):
                with self.assertRaises(ValueError): resolve_artifact(directory, name)
            (directory/'field.json').write_text('tampered')
            with self.assertRaises(ValueError): verify_bundle(directory)
            (directory/'field.json').unlink()
            with self.assertRaises(ValueError): verify_bundle(directory)
            (directory/'unexpected').write_text('extra')
            with self.assertRaises(ValueError): verify_bundle(directory)

    def test_nested_manifest_is_bound_and_cannot_hide_changes(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            nested = directory/'input'
            nested.mkdir()
            (nested/'bundle_manifest.json').write_text('{"source":"retained input"}')
            seal_bundle(directory)
            self.assertEqual(verify_bundle(directory)['files'], 1)
            (nested/'bundle_manifest.json').write_text('{"source":"changed"}')
            with self.assertRaises(ValueError):
                verify_bundle(directory)

    def test_cleanup_refuses_legacy_receipts_and_root_overlap(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            build = repo/'build'
            build.mkdir()
            protected = [repo/'data/experiments', repo/'data/tools', repo/'tmp/tests']
            check(repo, build, protected)
            for bad in (repo, repo.parent, repo/'data', repo/'data/experiments'):
                with self.assertRaises(ValueError): check(repo, bad, protected)
            (build/'geometry.npz').write_bytes(b'legacy saved geometry')
            with self.assertRaises(ValueError): check(repo, build, protected)
            (build/'geometry.npz').unlink()
            (build/'receipt.json').write_text('{"artifact_sha256":{}}')
            with self.assertRaises(ValueError): check(repo, build, protected)
            for bad in (repo/'build/retained', repo/'tmp/retained', repo):
                with self.assertRaises(ValueError): experiment_root(repo, bad)


if __name__ == '__main__':
    unittest.main()
