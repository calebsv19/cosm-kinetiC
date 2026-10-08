import fcntl,hashlib,os,resource,signal,subprocess,tempfile,unittest
from pathlib import Path
class ShapeMaskPublication(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binary=Path(os.environ['PHYSICS_SIM_SHAPE_OUTPUT_TEST_BIN']).resolve()
  cls.scene=Path(__file__).resolve().parents[1]/'import/u_shape.json'
  cls.digests={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in (cls.binary,cls.scene)}
 @classmethod
 def tearDownClass(cls):
  for p,d in cls.digests.items():
   if hashlib.sha256(p.read_bytes()).hexdigest()!=d:raise AssertionError(f'Changed input: {p}')
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='physics-shape-publish-',dir='/private/tmp');self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name);self.out=self.root/'out.pgm'
 def invoke(self,out=None,limit=False,source=None):
  def constrain():
   signal.signal(signal.SIGXFSZ,signal.SIG_IGN);resource.setrlimit(resource.RLIMIT_FSIZE,(64,64))
  return subprocess.run([str(self.binary),'--grid','32','32','--out',str(out or self.out),str(source or self.scene)],capture_output=True,text=True,timeout=3,preexec_fn=constrain if limit else None)
 def refuse(self,path):
  row=self.invoke(path);self.assertEqual(row.returncode,1,row.stdout+row.stderr);self.assertIn('Failed to write',row.stderr)
 def test_missing_parent_reports_failure(self):
  self.refuse(self.root/'missing/out');self.assertFalse((self.root/'missing').exists())
 def test_symlink_output_preserves_target(self):
  target=self.root/'target';target.write_bytes(b'original');self.out.symlink_to(target);self.refuse(self.out);self.assertEqual(target.read_bytes(),b'original');self.assertTrue(self.out.is_symlink())
 def test_hardlink_output_preserves_both_names(self):
  target=self.root/'target';target.write_bytes(b'original');os.link(target,self.out);self.refuse(self.out);self.assertEqual(target.read_bytes(),b'original');self.assertEqual(self.out.read_bytes(),b'original')
 def test_linked_parent_refuses(self):
  target=self.root/'real';target.mkdir();linked=self.root/'linked';linked.symlink_to(target,target_is_directory=True);self.refuse(linked/'out');self.assertEqual(list(target.iterdir()),[])
 def test_fifo_refuses_without_open_block(self):
  os.mkfifo(self.out);self.refuse(self.out)
 def test_directory_refuses(self):
  self.out.mkdir();(self.out/'sentinel').write_text('original');self.refuse(self.out);self.assertEqual((self.out/'sentinel').read_text(),'original')
 def test_cooperative_competitor_refuses(self):
  with (self.root/'.physics-sim-persistence.lock').open('wb') as lock:
   fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);self.refuse(self.out);self.assertFalse(self.out.exists())
 def test_write_failure_preserves_predecessor_and_stage(self):
  self.out.write_bytes(b'original predecessor');row=self.invoke(limit=True)
  self.assertEqual(self.out.read_bytes(),b'original predecessor');self.assertEqual(row.returncode,1,row.stdout+row.stderr)
  stages=list(self.root.glob('.headless-sidecar-*.pending'));self.assertEqual(len(stages),1);self.assertEqual(stages[0].stat().st_size,64)
 def test_input_output_alias_preserves_source(self):
  self.out.write_bytes(self.scene.read_bytes());before=self.out.read_bytes();row=self.invoke(source=self.out)
  self.assertEqual(self.out.read_bytes(),before);self.assertEqual(row.returncode,1,row.stderr);self.assertIn('aliases shape input',row.stderr)
 def test_git_source_storage_is_protected(self):
  (self.root/'.git').mkdir();(self.root/'src').mkdir();out=self.root/'src/saved';out.write_bytes(b'original');self.refuse(out);self.assertEqual(out.read_bytes(),b'original')
 def test_success_create_replace_and_mask_bytes(self):
  row=self.invoke();self.assertEqual(row.returncode,0,row.stderr);first=self.out.read_bytes();self.assertTrue(first.startswith(b'P5\n32 32\n255\n'));self.assertEqual(len(first.split(b'\n',3)[3]),1024)
  self.out.write_bytes(b'previous');row=self.invoke();self.assertEqual(row.returncode,0,row.stderr);self.assertEqual(self.out.read_bytes(),first)
  self.assertEqual(list(self.root.glob('.headless-sidecar-*.pending')),[])
if __name__=='__main__':unittest.main()
