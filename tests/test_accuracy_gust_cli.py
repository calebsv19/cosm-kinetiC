"""Output custody: refusal must preserve unrelated data and failed reproducers."""
import importlib.util,json,os,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CLI=ROOT/'scripts/fire_fluid_accuracy_gust.py'
def module():
    spec=importlib.util.spec_from_file_location('accuracy_cli_custody_fixture',CLI)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
class AccuracyGustCliCustody(unittest.TestCase):
    def test_existing_directory_and_dangling_link_are_preserved(self):
        with tempfile.TemporaryDirectory(prefix='accuracy-cli-',dir=os.environ.get('ACCURACY_TEST_ROOT')) as tmp:
            base=Path(tmp).resolve();old=base/'old';old.mkdir();sentinel=old/'error.json';sentinel.write_bytes(b'user-owned failure evidence')
            link=base/'link';target=base/'never-created';link.symlink_to(target,target_is_directory=True)
            for output in (old,link):
                with self.subTest(output=output.name):
                    workers=Path(os.environ['REPLAY_WORKERS']) if output==old else base/'missing-workers'
                    pulse=Path(os.environ['TIMING_PULSE']) if output==old else base/'missing-pulse'
                    p=subprocess.run([sys.executable,'-B',str(CLI),'--workers',str(workers),'--pulse',str(pulse),'--references',os.environ['ACCURACY_REFERENCE_GATE'],'--output',str(output),'--budget-root',str(base)],capture_output=True,text=True)
                    self.assertNotEqual(p.returncode,0);self.assertIn('fresh output root',p.stderr)
                    self.assertEqual(sentinel.read_bytes(),b'user-owned failure evidence');self.assertTrue(link.is_symlink());self.assertFalse(target.exists())
    def test_wrong_owner_failure_does_not_overwrite(self):
        m=module()
        with tempfile.TemporaryDirectory(prefix='accuracy-cli-',dir=os.environ.get('ACCURACY_TEST_ROOT')) as tmp:
            root=Path(tmp).resolve();(root/'process.json').write_text(json.dumps({'pid':os.getpid()+1,'root':str(root),'script':str(ROOT/'scripts/coupling/accuracy_gust_study.py')}));p=root/'error.json';p.write_bytes(b'prior-owner-error')
            self.assertFalse(m.record_owned_failure(root,RuntimeError('new error')));self.assertEqual(p.read_bytes(),b'prior-owner-error')
    def test_owned_failure_records_exact_reproducer(self):
        m=module()
        with tempfile.TemporaryDirectory(prefix='accuracy-cli-',dir=os.environ.get('ACCURACY_TEST_ROOT')) as tmp:
            root=Path(tmp).resolve();(root/'process.json').write_text(json.dumps({'pid':os.getpid(),'root':str(root),'script':str(ROOT/'scripts/coupling/accuracy_gust_study.py')}))
            try:raise RuntimeError('controlled native failure fixture')
            except RuntimeError as exc:self.assertTrue(m.record_owned_failure(root,exc))
            d=json.loads((root/'error.json').read_text());self.assertEqual(d['error'],'controlled native failure fixture');self.assertTrue(d['preserved']);self.assertIn('RuntimeError',d['traceback'])
if __name__=='__main__':unittest.main()
