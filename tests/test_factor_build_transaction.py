"""Real staged library compilation and exact create-only factor record binding."""
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import cfd_run_support as c

class FactorBuild(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve()
    def compile(self):
        source=self.root/'factor.c';source.write_text('int controlled_factor_fixture(void){return 7;}\n')
        self.command=['/usr/bin/clang','-dynamiclib',str(source),'-o',str(self.root/'factor.dylib')]
        result=c.compile_probe(self.command,self.root,'compile',20,1024**3,cwd=self.root)
        self.metadata=json.dumps({'command':self.command,'library_sha256':result['output_sha256'],'source_sha256':c.sha(source)})
        return result
    def test_actual_library_record_reuse_and_install_identity(self):
        result=self.compile();path=self.root/'factor-build.json'
        c.publish_factor_record(path,self.metadata,result);before=path.read_bytes()
        record=c.read_factor_record(path)
        self.assertEqual(record['command'],self.command)
        self.assertEqual(record['factor_record_schema'],'physics_sim_factor_build_v2')
        self.assertFalse(record['all_external_descendants_verified_terminal'])
        self.assertEqual(ctypes.CDLL(str(self.root/'factor.dylib')).controlled_factor_fixture(),7)
        identity=subprocess.run(['/usr/bin/otool','-D',str(self.root/'factor.dylib')],capture_output=True,text=True,check=True).stdout
        self.assertIn(str(self.root/'factor.dylib'),identity);self.assertNotIn('.compiler-attempts',identity)
        self.assertEqual(path.read_bytes(),before)
    def test_failed_compile_retains_diagnostics_without_library_or_record(self):
        source=self.root/'bad.c';source.write_text('invalid C;\n')
        with self.assertRaises(ValueError):c.compile_probe(['/usr/bin/clang','-dynamiclib',str(source),'-o',str(self.root/'factor.dylib')],self.root,'compile',20,1024**3)
        self.assertFalse((self.root/'factor.dylib').exists());self.assertFalse((self.root/'factor-build.json').exists())
        attempt=next((self.root/'.compiler-attempts').iterdir())
        self.assertEqual(json.loads((attempt/'receipt.json').read_text())['status'],'failed')
        self.assertGreater((self.root/'compile.stderr').stat().st_size,0)
    def test_existing_record_is_not_replaced_and_candidate_is_retained(self):
        result=self.compile();path=self.root/'factor-build.json';path.write_bytes(b'unknown prior record')
        with self.assertRaises(FileExistsError):c.publish_factor_record(path,self.metadata,result)
        self.assertEqual(path.read_bytes(),b'unknown prior record')
        attempt=(self.root/result['compile_receipt']).parent
        self.assertTrue((attempt/'factor-build.json.candidate').exists())
    def test_changed_library_or_proof_is_held_without_record_mutation(self):
        result=self.compile();path=self.root/'factor-build.json';c.publish_factor_record(path,self.metadata,result);before=path.read_bytes()
        library=self.root/'factor.dylib';original=library.read_bytes();library.write_bytes(b'changed')
        with self.assertRaises(ValueError):c.read_factor_record(path)
        library.write_bytes(original)
        proof=self.root/result['compile_receipt'];proof.write_bytes(proof.read_bytes()+b' ')
        with self.assertRaises(ValueError):c.read_factor_record(path)
        self.assertEqual(path.read_bytes(),before)
    def test_metadata_duplicates_links_specials_and_bounds_are_held(self):
        p=self.root/'metadata.json';p.write_text('{"key":1,"key":2}')
        with self.assertRaises(ValueError):c.factor_json(p)
        p.write_bytes(b'x'*65537)
        with self.assertRaises(ValueError):c.factor_json(p)
        link=self.root/'link';link.symlink_to(p)
        with self.assertRaises(ValueError):c.factor_digest(link)
        fifo=self.root/'fifo';os.mkfifo(fifo)
        with self.assertRaises(ValueError):c.factor_digest(fifo)
    def test_tool_identity_output_is_bounded_and_retained(self):
        row=c.factor_tool(['/usr/bin/clang','--version'],self.root,'identity',cwd=self.root)
        self.assertIn('clang',row.stdout.lower());self.assertTrue((self.root/'identity.stderr').exists())

if __name__=='__main__':unittest.main()
