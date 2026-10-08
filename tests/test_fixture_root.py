"""Fresh owned fixture roots preserve parents and reject protected namespaces."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from fixture_root import allocate

class FixtureRoot(unittest.TestCase):
    def test_concurrent_invocations_preserve_existing_parent_and_each_other(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();parent=repo/'tmp/tests/custom';parent.mkdir(parents=True)
            (parent/'prior-diagnostics').write_bytes(b'preserved')
            with ThreadPoolExecutor(max_workers=8) as pool:
                roots=list(pool.map(lambda n:allocate(repo,parent,'water_fixture'),range(16)))
            self.assertEqual(len(set(roots)),16)
            identities=set()
            for root in roots:
                self.assertEqual(list(root.iterdir()),[])
                receipt=json.loads((root.parent/'fixture_owner.json').read_text())
                self.assertEqual(receipt['root'],str(root));self.assertFalse(receipt['parent_reset_performed'])
                identities.add(receipt['invocation_id'])
            self.assertEqual(len(identities),16)
            self.assertEqual((parent/'prior-diagnostics').read_bytes(),b'preserved')

    def test_protected_outside_and_symlink_parents_refused_before_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve();(repo/'tmp').mkdir();(repo/'tmp/alias').symlink_to(repo/'src')
            before=set(repo.rglob('*'))
            for parent in (repo/'src',repo/'build',repo/'data/experiments',repo/'tmp/alias',repo.parent/'outside'):
                with self.assertRaises(ValueError):allocate(repo,parent,'fixture')
            self.assertEqual(set(repo.rglob('*')),before)

    def test_retained_visual_namespace_is_explicit(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary).resolve()
            with self.assertRaises(ValueError):allocate(repo,repo/'visual_artifacts','fixture')
            root=allocate(repo,repo/'visual_artifacts','fixture',retained=True)
            receipt=json.loads((root.parent/'fixture_owner.json').read_text())
            self.assertEqual(receipt['artifact_class'],'fixture_visual_output')
            self.assertFalse(receipt['automatic_removal'])

if __name__=='__main__':unittest.main()
