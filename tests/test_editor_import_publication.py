"""Native editor conversion publication in disposable roots; explicit binary required."""
import fcntl, json, os, resource, signal, subprocess, tempfile, unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
class EditorImportPublication(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = Path(os.environ['PHYSICS_SIM_EDITOR_IMPORT_TEST_BIN']).resolve()
        cls.scene = ROOT/'import/u_shape.json'
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='physics-editor-import-', dir='/private/tmp')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root/'.git').mkdir()
        (self.root/'config/objects').mkdir(parents=True)
        (self.root/'data/runtime').mkdir(parents=True)
        self.out = self.root/'data/runtime/u_shape.asset.json'
    def invoke(self, source=None, capacity=1024, directory=None, limit=False):
        env = dict(os.environ); env.pop('SHAPE_ASSET_DIR',None)
        if directory is not None: env['SHAPE_ASSET_DIR'] = str(directory)
        def constrain():
            signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE,(64,64))
        return subprocess.run([str(self.binary),str(source or self.scene),str(self.root/'config'),str(capacity)], cwd=self.root,env=env,capture_output=True,text=True,timeout=3,preexec_fn=constrain if limit else None)
    def refuse(self, **kwargs):
        row=self.invoke(**kwargs);self.assertEqual(row.returncode,1,row.stdout+row.stderr);self.assertEqual(row.stdout,'\n')
    def test_default_creates_generated_asset_and_preserves_source_assets(self):
        (self.root/'config/objects/u_shape.asset.json').write_text('source sentinel')
        row=self.invoke();self.assertEqual(row.returncode,0,row.stderr)
        self.assertEqual(row.stdout.strip(),'data/runtime/u_shape.asset.json');self.assertIsInstance(json.loads(self.out.read_bytes()),dict)
        self.assertEqual((self.root/'config/objects/u_shape.asset.json').read_text(),'source sentinel')
    def test_regenerates_existing_malformed_cache(self):
        self.out.write_text('malformed');row=self.invoke();self.assertEqual(row.returncode,0,row.stderr);self.assertIsInstance(json.loads(self.out.read_bytes()),dict)
    def test_existing_cache_does_not_hide_invalid_input(self):
        self.out.write_text('predecessor');source=self.root/'u_shape.json';source.write_text('invalid')
        self.refuse(source=source);self.assertEqual(self.out.read_text(),'predecessor')
    def test_tiny_return_buffer_refuses_before_write(self):
        self.refuse(capacity=8);self.assertFalse(self.out.exists());self.assertEqual(list((self.root/'data/runtime').iterdir()),[])
    def test_zero_return_buffer_refuses(self):
        row=self.invoke(capacity=0);self.assertEqual(row.returncode,1);self.assertFalse(self.out.exists())
    def test_missing_runtime_is_created(self):
        (self.root/'data/runtime').rmdir();(self.root/'data').rmdir();row=self.invoke();self.assertEqual(row.returncode,0,row.stderr);self.assertTrue(self.out.is_file())
    def test_linked_runtime_refuses(self):
        (self.root/'data/runtime').rmdir();target=self.root/'target';target.mkdir();(self.root/'data/runtime').symlink_to(target,target_is_directory=True)
        self.refuse();self.assertEqual(list(target.iterdir()),[])
    def test_linked_destination_preserves_target(self):
        target=self.root/'target';target.write_text('original');self.out.symlink_to(target);self.refuse();self.assertEqual(target.read_text(),'original')
    def test_hardlinked_destination_preserves_both(self):
        target=self.root/'target';target.write_text('original');os.link(target,self.out);self.refuse();self.assertEqual(target.read_text(),'original')
    def test_fifo_refuses_without_block(self):
        os.mkfifo(self.out);self.refuse()
    def test_lock_conflict_preserves_predecessor(self):
        self.out.write_text('original')
        with (self.out.parent/'.physics-sim-persistence.lock').open('wb') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);self.refuse()
        self.assertEqual(self.out.read_text(),'original')
    def test_write_fault_retains_predecessor_and_failed_candidate(self):
        self.out.write_text('original');self.refuse(limit=True);self.assertEqual(self.out.read_text(),'original')
        stages=list(self.out.parent.glob('.headless-sidecar-*.pending'));self.assertEqual(len(stages),1);self.assertEqual(stages[0].stat().st_size,64)
    def test_source_directory_override_refuses(self):
        self.refuse(directory=self.root/'config/objects');self.assertEqual(list((self.root/'config/objects').iterdir()),[])
    def test_external_override_and_missing_parent(self):
        external=Path(self.temp.name+'-external');external.mkdir();self.addCleanup(__import__('shutil').rmtree,external)
        row=self.invoke(directory=external);self.assertEqual(row.returncode,0,row.stderr);self.assertTrue((external/'u_shape.asset.json').is_file())
        self.refuse(directory=external/'missing');self.assertFalse((external/'missing').exists())
    def test_long_output_directory_refuses_before_write(self):
        self.refuse(directory='x'*600);self.assertFalse(self.out.exists())
    def test_model_path_capacity_refuses_before_write(self):
        # Fits the 512-byte conversion temporary and 1024-byte return buffer,
        # but cannot fit ImportedShape.path (256 bytes).
        self.refuse(directory='x'*250);self.assertFalse(self.out.exists())
    def test_directory_destination_refuses(self):
        self.out.mkdir();(self.out/'sentinel').write_text('original');self.refuse();self.assertEqual((self.out/'sentinel').read_text(),'original')
if __name__=='__main__':unittest.main()
