import hashlib,json,os,subprocess,tempfile,unittest
from pathlib import Path

class ShapeNumericArguments(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[1] if Path(__file__).parent.name=='tests' else Path('/Users/calebsv/Desktop/CodeWork/_worktrees/physics_sim_main_edit')
        cls.bins=[Path(os.environ[k]).resolve() for k in ('PHYSICS_SIM_SHAPE_MASK_TEST_BIN','PHYSICS_SIM_SHAPE_ASSET_TEST_BIN')]
        cls.scene=cls.repo/'import/u_shape.json'
        cls.identities={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in cls.bins+[cls.scene]}
    @classmethod
    def tearDownClass(cls):
        for p,digest in cls.identities.items():
            if hashlib.sha256(p.read_bytes()).hexdigest()!=digest:raise AssertionError(f'Input changed: {p}')
    def invoke(self,tool,args,source,output):
        return subprocess.run([str(self.bins[tool]),*args,'--out',str(output),str(source)],capture_output=True,text=True,timeout=5)
    def rejected(self,tool,args):
        with tempfile.TemporaryDirectory(prefix='physics-shape-args-') as td:
            root=Path(td); out=root/'existing';out.write_bytes(b'preserved predecessor')
            row=self.invoke(tool,args,root/'missing.json',out)
            self.assertEqual(row.returncode,1,row.stderr)
            self.assertIn('Invalid',row.stderr)
            self.assertNotIn('Failed to load',row.stderr)
            self.assertEqual(row.stdout,'')
            self.assertEqual(out.read_bytes(),b'preserved predecessor')
    def test_grid_integer_and_allocation_admission(self):
        for values in [('0','32'),('-1','32'),('4294967328','32'),('9223372036854775808','32'),('32junk','32'),('32','0'),('2147483647','2147483647'),('8193','8193')]:
            with self.subTest(values=values):self.rejected(0,['--grid',*values])
    def test_mask_float_representation(self):
        for option in ['--margin','--stroke','--max-error','--rot','--scale']:
            for value in ['nan','inf','-inf','1e999','1e-999','1tail']:
                with self.subTest(option=option,value=value):self.rejected(0,[option,value])
        for values in [('nan','0.5'),('0.5','inf')]:
            with self.subTest(values=values):self.rejected(0,['--pos',*values])
    def test_mask_option_domains(self):
        for args in [['--margin','-1'],['--stroke','0'],['--stroke','-1'],['--max-error','0'],['--max-error','-1'],['--scale','0'],['--scale','-1'],['--pos','-0.1','0.5'],['--pos','0.5','1.1']]:
            with self.subTest(args=args):self.rejected(0,args)
    def test_asset_tolerance_admission(self):
        for value in ['nan','inf','-inf','1e999','1e-999','0','-1','0.5junk']:
            with self.subTest(value=value):self.rejected(1,['--max-error',value])
    def test_invalid_arguments_refuse_before_fifo_input(self):
        with tempfile.TemporaryDirectory(prefix='physics-shape-fifo-') as td:
            root=Path(td);fifo=root/'input';os.mkfifo(fifo)
            for tool,args in [(0,['--grid','0','32']),(1,['--max-error','nan'])]:
                with self.subTest(tool=tool):
                    row=self.invoke(tool,args,fifo,root/'out')
                    self.assertEqual(row.returncode,1,row.stderr);self.assertIn('Invalid',row.stderr)
                    self.assertFalse((root/'out').exists())
    def test_valid_mask_and_asset_conversion(self):
        with tempfile.TemporaryDirectory(prefix='physics-shape-valid-') as td:
            root=Path(td);mask=root/'out.pgm';asset=root/'out.json'
            row=self.invoke(0,['--grid','32','32','--margin','0','--stroke','1','--pos','0','1','--rot','-90','--scale','1e0'],self.scene,mask)
            self.assertEqual(row.returncode,0,row.stderr);self.assertTrue(mask.read_bytes().startswith(b'P5\n32 32\n255\n'))
            self.assertEqual(len(mask.read_bytes().split(b'\n',3)[3]),1024)
            row=self.invoke(1,['--max-error','0.5'],self.scene,asset)
            self.assertEqual(row.returncode,0,row.stderr);self.assertIsInstance(json.loads(asset.read_text()),dict)
    def test_overflow_grid_does_not_wrap_into_successful_conversion(self):
        with tempfile.TemporaryDirectory(prefix='physics-shape-wrap-') as td:
            out=Path(td)/'out.pgm';row=self.invoke(0,['--grid','4294967328','32'],self.scene,out)
            self.assertEqual(row.returncode,1,row.stderr);self.assertIn('Invalid',row.stderr);self.assertFalse(out.exists())
if __name__=='__main__':unittest.main()
