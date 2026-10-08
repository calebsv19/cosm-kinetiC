"""Pre-allocation reference roots, source freezing and retained-path holds."""
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import cfd_run_support as c
from test_reference_supervision import ReferenceSupervision
FAMILIES=json.loads((ROOT/'config/reference_supervision_families.json').read_text())['families']+json.loads((ROOT/'config/reference_factor_supervision_families.json').read_text())['families']

class Roots(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.base=Path(self.tmp.name).resolve();self.repo=self.base/'repo';self.repo.mkdir();(self.repo/'scripts').mkdir()
        self.source=self.repo/'scripts/probe.py';self.source.write_bytes(b'# frozen source\n');self.runner=self.base/'runner.py';self.runner.write_bytes(b'# runner\n')
        self.sources=('probe.py',);self.data=self.repo/'build/c3d-test/runs'
    def prepare(self):return c.reference_prepare(self.repo,self.data,self.sources,self.runner)
    def directory(self):
        hashes={'probe.py':c.factor_digest(self.source)}
        return self.data/hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    def test_matching_preparation_reuses_bytes_without_replacement(self):
        hashes,digest,directory,frozen=self.prepare()
        before={p:(p.stat().st_ino,p.read_bytes()) for p in (frozen/'probe.py',self.data.parent/'supervisor-source'/(c.factor_digest(self.runner)+'.py'))}
        self.assertEqual(self.prepare(),(hashes,digest,directory,frozen))
        for p,value in before.items():self.assertEqual((p.stat().st_ino,p.read_bytes()),value)
    def test_protected_outside_and_traversal_roots_hold_before_allocation(self):
        for data in (self.repo/'src/runs',self.repo/'build',self.base/'outside/runs',self.repo/'build/c3d-test/../runs'):
            self.data=data
            with self.assertRaises(ValueError):self.prepare()
        self.assertFalse((self.repo/'build').exists());self.assertFalse((self.base/'outside').exists())
    def test_linked_ancestors_and_run_source_supervisor_hold_without_target_writes(self):
        outside=self.base/'outside';outside.mkdir();(outside/'sentinel').write_bytes(b'keep')
        for kind in ('ancestor','run','source','supervisor'):
            with self.subTest(kind=kind):
                if kind=='ancestor':link=self.repo/'build'
                elif kind=='run':link=self.directory()
                elif kind=='source':link=self.directory()/'source'
                else:link=self.data.parent/'supervisor-source'
                link.parent.mkdir(parents=True,exist_ok=True);link.symlink_to(outside,target_is_directory=True)
                with self.assertRaises(ValueError):self.prepare()
                self.assertEqual(list(outside.iterdir()),[outside/'sentinel']);self.assertEqual((outside/'sentinel').read_bytes(),b'keep')
                link.unlink()
    def test_special_and_changed_retained_entries_hold_before_source_allocation(self):
        d=self.directory();d.mkdir(parents=True)
        fifo=d/'unexpected';os.mkfifo(fifo)
        with self.assertRaises(ValueError):self.prepare()
        self.assertFalse((d/'source').exists());fifo.unlink()
        frozen=d/'source';frozen.mkdir();p=frozen/'probe.py';p.write_bytes(b'changed')
        with self.assertRaises(ValueError):self.prepare()
        self.assertEqual(p.read_bytes(),b'changed');self.assertFalse((self.data.parent/'supervisor-source').exists())
    def test_linked_or_special_checkout_sources_hold_before_allocation(self):
        self.source.unlink();self.source.symlink_to(self.runner)
        with self.assertRaises(ValueError):self.prepare()
        self.source.unlink();os.mkfifo(self.source)
        with self.assertRaises(ValueError):self.prepare()
        self.assertFalse(self.data.exists())
    def test_all_families_hold_linked_output_roots_before_writes_or_compilation(self):
        self.assertEqual(len(FAMILIES),194)
        outside=self.base/'outside';outside.mkdir()
        for rel in FAMILIES:
            with self.subTest(family=rel):
                m,repo=ReferenceSupervision().prepare(self.base,rel)
                m.DATA.parent.mkdir(parents=True,exist_ok=True);m.DATA.symlink_to(outside,target_is_directory=True)
                with self.assertRaises(ValueError):m.run('proof',['--fixture-exit','0'])
                self.assertFalse(list(outside.iterdir()))
    def test_all_families_hold_linked_sources_before_output_allocation(self):
        for rel in FAMILIES:
            with self.subTest(family=rel):
                m,repo=ReferenceSupervision().prepare(self.base,rel)
                p=repo/'scripts'/m.SOURCES[0];p.unlink();p.symlink_to(self.runner)
                with self.assertRaises(ValueError):m.run('proof',['--fixture-exit','0'])
                self.assertFalse(m.DATA.exists())

if __name__=='__main__':unittest.main()
