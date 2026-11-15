"""Owned external slots preserve source and reject unrelated storage."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
import output_layout as layout
import build_owner
import build_outputs
import fixture_root
import package_outputs
import package_paths


class Layout(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.repo = self.root/'source/physics_sim'
        self.repo.mkdir(parents=True)
        for name in ('build', 'runtime', 'tests', 'package'):
            (self.root/name).mkdir()
        self.descriptor = self.root/'descriptor.json'
        self.descriptor.write_text(json.dumps({'attempt_root': str(self.root),
            'job_id': 'rapcj_'+'a'*64, 'lease_id': 'rapcl_'+'b'*64,
            'inputs_sha256': 'c'*64}))
        info = self.root.stat()
        self.marker = self.root/'.task-output-owner.json'
        self.marker.write_text(json.dumps({'schema': 'owned-task-output/v1',
            'root': str(self.root), 'device': info.st_dev, 'inode': info.st_ino,
            'owner': str(self.descriptor)}))
        self.env = patch.dict(os.environ, {'PHYSICS_SIM_LAYOUT_BUILD': str(self.root/'build')})
        self.env.start(); self.addCleanup(self.env.stop)

    def test_compile_ownership_and_receipts_use_external_slots(self):
        descriptors = build_owner.acquire(self.repo, self.root/'build')
        try:
            with self.assertRaises(ValueError):
                build_owner.acquire(self.repo, self.root/'build')
            output = self.root/'build/probe.o'; output.write_bytes(b'owned output')
            build_outputs.record(self.repo, output, 'compiler')
            self.assertEqual(build_outputs.verified(self.repo, output)['path'], str(output))
            self.assertTrue(build_outputs.receipt_path(self.repo, output).is_relative_to(self.root/'runtime'))
            output.write_bytes(b'changed')
            with self.assertRaises(ValueError): build_outputs.verified(self.repo, output)
        finally:
            for fd in descriptors: os.close(fd)
        self.assertEqual(list(self.repo.iterdir()), [])

    def test_fixtures_and_packages_preserve_source(self):
        fixture = fixture_root.allocate(self.repo, self.root/'tests', 'layout-probe')
        self.assertTrue(fixture.is_relative_to(self.root/'tests'))
        root = self.root/'package/gate'
        output = root/'Example.app'
        validate = lambda: package_outputs.plan(self.repo, root, [output], [])
        result = package_outputs.declare(validate(), validate)
        row = result['outputs'][0]
        receipt = Path(row['reservation'])
        self.assertEqual(package_paths.reservation_output(self.repo, receipt, json.loads(receipt.read_text())), output)
        with self.assertRaises(ValueError): package_outputs.declare(validate(), validate)
        self.assertEqual(list(self.repo.iterdir()), [])

    def test_unrelated_paths_and_build_package_overlap_refused(self):
        for path in (self.root, self.root/'other', self.root.parent/'unrelated'):
            with self.assertRaises(ValueError): layout.no_symlinks(path, self.repo)
        with self.assertRaises(ValueError):
            package_outputs.plan(self.repo, self.root/'build/bin', [self.root/'build/bin/app'], [])
        with self.assertRaises(ValueError):
            fixture_root.allocate(self.repo, self.root/'runtime', 'wrong-slot')

    def test_marker_descriptor_and_symlink_drift_refused(self):
        original = self.marker.read_text(); value = json.loads(original)
        value['inode'] += 1; self.marker.write_text(json.dumps(value))
        with self.assertRaises(ValueError): layout.build_root(self.repo)
        self.marker.write_text(original)
        descriptor = json.loads(self.descriptor.read_text()); descriptor['attempt_root'] = '/'
        self.descriptor.write_text(json.dumps(descriptor))
        with self.assertRaises(ValueError): layout.build_root(self.repo)

    def test_symlink_and_other_source_refused(self):
        (self.root/'build/link').symlink_to(self.root/'tests', target_is_directory=True)
        with self.assertRaises(ValueError): layout.no_symlinks(self.root/'build/link/out', self.repo)
        with self.assertRaises(ValueError): layout.build_root(self.root/'source/other')

    def test_ordinary_checkout_defaults_remain_local(self):
        with patch.dict(os.environ, {'PHYSICS_SIM_LAYOUT_BUILD': ''}):
            self.assertEqual(layout.build_root(self.repo), self.repo/'build')
            self.assertEqual(layout.runtime_root(self.repo), self.repo/'tmp')
            with self.assertRaises(ValueError): layout.no_symlinks(self.root/'build', self.repo)


if __name__ == '__main__': unittest.main()
