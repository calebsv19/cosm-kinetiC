"""Failed or competing probe compilation never replaces retained binaries."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
from cfd_run_support import compile_probe
from cfd_evidence import seal_bundle, verify_bundle


class RetainedCompile(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()

    def command(self, code):
        return [sys.executable, '-c', code, '-o', str(self.root/'probe')]

    def test_success_has_verified_publication_and_failed_retry_preserves_original(self):
        command = self.command("import pathlib,sys;pathlib.Path(sys.argv[-1]).write_bytes(b'good probe')")
        result = compile_probe(command,self.root,'compile',10,512*1024**2)
        receipt = json.loads((self.root/result['compile_receipt']).read_text())
        self.assertEqual(receipt['status'],'passed')
        self.assertEqual((self.root/'probe').read_bytes(),b'good probe')
        with self.assertRaisesRegex(ValueError,'already exists'):
            compile_probe(command,self.root,'retry',10,512*1024**2)
        self.assertEqual((self.root/'probe').read_bytes(),b'good probe')
        seal_bundle(self.root);verify_bundle(self.root)

    def test_failed_compilation_retains_candidate_without_final_publication(self):
        command=self.command("import pathlib,sys;pathlib.Path(sys.argv[-1]).write_bytes(b'partial');sys.exit(7)")
        with self.assertRaises(ValueError):compile_probe(command,self.root,'compile',10,512*1024**2)
        self.assertFalse((self.root/'probe').exists())
        attempt=next((self.root/'.compiler-attempts').iterdir())
        self.assertEqual((attempt/'probe').read_bytes(),b'partial')
        self.assertEqual(json.loads((attempt/'receipt.json').read_text())['status'],'failed')
        self.assertTrue((self.root/'compile.stderr').exists());seal_bundle(self.root);verify_bundle(self.root)

    def test_competing_publication_is_never_overwritten(self):
        command=self.command("import pathlib,sys;pathlib.Path('probe').write_bytes(b'other owner');pathlib.Path(sys.argv[-1]).write_bytes(b'candidate')")
        with self.assertRaises(FileExistsError):compile_probe(command,self.root,'compile',10,512*1024**2)
        self.assertEqual((self.root/'probe').read_bytes(),b'other owner')
        attempt=next((self.root/'.compiler-attempts').iterdir())
        self.assertEqual((attempt/'probe').read_bytes(),b'candidate')
        self.assertEqual(json.loads((attempt/'receipt.json').read_text())['status'],'failed')

    def test_output_escape_and_existing_symlink_are_refused(self):
        outside=self.root.parent/'outside-owned-probe'
        command=[sys.executable,'-c',"raise Exception('must not run')",'-o',str(outside)]
        with self.assertRaises(ValueError):compile_probe(command,self.root,'compile',10,512*1024**2)
        (self.root/'probe').symlink_to(self.root/'missing')
        with self.assertRaises(ValueError):compile_probe(self.command("raise Exception('must not run')"),self.root,'compile',10,512*1024**2)
        self.assertFalse((self.root/'compile.stdout').exists())

    def test_actual_clang_compiles_and_runs_retained_probe(self):
        source=self.root/'probe.c';source.write_text('int main(void){return 7;}\n')
        result=compile_probe(['clang',str(source),'-o',str(self.root/'probe')],self.root,'compile',20,512*1024**2)
        import subprocess
        self.assertEqual(subprocess.run([str(self.root/'probe')]).returncode,7)
        self.assertEqual(result['exit_code'],0)
        seal_bundle(self.root);verify_bundle(self.root)


if __name__ == '__main__':unittest.main()
