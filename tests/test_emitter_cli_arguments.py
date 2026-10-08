"""Acceptance checks against an explicitly selected real emitter diagnostic binary."""
import hashlib,os,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class EmitterArguments(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=os.environ.get('PHYSICS_SIM_EMITTER_ARGUMENT_TEST_BIN')
        if not raw:raise unittest.SkipTest('Explicit native emitter argument-test binary required')
        cls.binary=Path(raw).resolve(strict=True);cls.binary_digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
        cls.scene=ROOT/'tests/fixtures/scene_runtime_launch_projection.json';cls.scene_digest=hashlib.sha256(cls.scene.read_bytes()).hexdigest()
        cls.tmp=tempfile.TemporaryDirectory();cls.cwd=Path(cls.tmp.name);cls.addClassCleanup(cls.tmp.cleanup)
    @classmethod
    def tearDownClass(cls):
        if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.binary_digest:raise AssertionError('Test binary changed')
        if hashlib.sha256(cls.scene.read_bytes()).hexdigest()!=cls.scene_digest:raise AssertionError('Test scene changed')
    def invoke(self,*args):return subprocess.run([str(self.binary),*args],cwd=self.cwd,capture_output=True,text=True,timeout=5)
    def invalid(self,value,scene=None):
        row=self.invoke(str(scene or self.scene),value);self.assertEqual(row.returncode,1,row.stdout+row.stderr);self.assertIn('invalid emitter_index',row.stderr);self.assertNotIn('domain:',row.stdout);return row
    def test_malformed_decimal_strings_are_refused(self):
        for value in ['','abc','0tail','0x0','+0','-0',' 0','0 ','1e0','0.0','nan','inf','--help']:
            with self.subTest(value=value):self.invalid(value)
    def test_overflow_and_capacity_are_refused(self):
        for value in ['32','4294967296','18446744073709551616','9'*4096,'0'*33]:
            with self.subTest(value=value[:40]):self.invalid(value)
    def test_invalid_index_precedes_missing_scene_read(self):
        row=self.invalid('bad',self.cwd/'missing.json');self.assertNotIn('bridge apply failed',row.stderr)
    def test_invalid_index_precedes_fifo_scene_open(self):
        fifo=self.cwd/'scene.pipe';os.mkfifo(fifo);self.invalid('bad',fifo)
    def test_default_and_explicit_zero_run_real_diagnostic(self):
        for args in [[],['0']]:
            with self.subTest(args=args):
                row=self.invoke(str(self.scene),*args);self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertIn('selected_emitter_index: 0',row.stdout);self.assertIn('emitter step stats:',row.stdout)
    def test_leading_zero_decimal_remains_supported(self):
        row=self.invoke(str(self.scene),'0000');self.assertEqual(row.returncode,0,row.stderr);self.assertIn('selected_emitter_index: 0',row.stdout)
    def test_in_capacity_index_still_checks_actual_scene_count(self):
        row=self.invoke(str(self.scene),'31');self.assertEqual(row.returncode,1,row.stderr);self.assertIn('emitter_index=31 out of range',row.stderr)
    def test_usage_arity_is_preserved(self):
        for args in [[],[str(self.scene),'0','extra']]:
            with self.subTest(args=args):
                row=self.invoke(*args);self.assertEqual(row.returncode,1);self.assertIn('usage:',row.stderr)
if __name__=='__main__':unittest.main()
