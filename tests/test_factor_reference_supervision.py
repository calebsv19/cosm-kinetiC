"""Factor wrapper command control with real Clang and explicitly controlled C fixtures."""
import contextlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from test_reference_supervision import ReferenceSupervision,ROOT
FAMILIES=json.loads((ROOT/'config/reference_factor_supervision_families.json').read_text())['families']

class FactorReference(unittest.TestCase):
    def prepare(self,base,relative):
        m,repo=ReferenceSupervision().prepare(base,relative)
        # Real Clang/SDK recipe execution with small controlled C sources.
        # These libraries do not implement or qualify the CFD factor ABI.
        for name in m.SOURCES:
            if name.endswith('.c'):
                (repo/'scripts'/name).write_text('int controlled_factor_fixture(void){return 7;}\n')
        return m,repo
    def test_every_factor_family_success_nonzero_and_cached_readback(self):
        self.assertEqual(len(FAMILIES),139)
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp).resolve()
            for relative in FAMILIES:
                m,repo=self.prepare(base,relative)
                for code in (0,2):
                    with self.subTest(family=relative,code=code):
                        name='proof-'+str(code)
                        with contextlib.redirect_stdout(io.StringIO()):
                            self.assertEqual(m.run(name,['--fixture-exit',str(code)]),code==0)
                        p=next(m.DATA.glob('*/'+name+'-receipt.json'));before=p.read_bytes();receipt=json.loads(before)
                        self.assertEqual(receipt['returncode'],code);self.assertIsNone(receipt['stop_reason'])
                        self.assertFalse(receipt['supervision']['all_external_descendants_verified_terminal'])
                        self.assertIn('cfd_run_support.py',receipt['source_sha256'])
                        self.assertIn('retained diagnostic stderr',(p.parent/(name+'.log')).read_text())
                        build_records={q:q.read_bytes() for q in (p.parent/'source').glob('*build.json')}
                        self.assertTrue(build_records)
                        for q,data in build_records.items():
                            record=json.loads(data)
                            self.assertEqual(record['factor_record_schema'],'physics_sim_factor_build_v2')
                            self.assertTrue((q.parent/record['compile_receipt']).is_file())
                        with contextlib.redirect_stdout(io.StringIO()):
                            self.assertEqual(m.run(name,['--fixture-exit',str(code)]),code==0)
                        self.assertEqual(p.read_bytes(),before)
                        altered=json.loads(before);del altered['artifact_sha256'][str(p.parent/(name+'.log'))]
                        p.write_text(json.dumps(altered))
                        with contextlib.redirect_stdout(io.StringIO()),self.assertRaises(ValueError):m.run(name,['--fixture-exit',str(code)])
                        p.write_bytes(before)
                        for q,data in build_records.items():self.assertEqual(q.read_bytes(),data)
    def test_invalid_names_refuse_before_factor_build_or_allocation(self):
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp).resolve()
            for relative in FAMILIES:
                with self.subTest(family=relative):
                    m,repo=self.prepare(base,relative)
                    with self.assertRaises(ValueError):m.run('../escape',[])
                    self.assertFalse(m.DATA.exists())

if __name__=='__main__':unittest.main()
