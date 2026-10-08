"""Execute frozen reference wrapper control contracts with synthetic solver output."""
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
FAMILIES=json.loads((ROOT/'config/reference_supervision_families.json').read_text())['families']
PROBE="""import sys,json,pathlib
args=sys.argv[1:]
code=int(args[args.index('--fixture-exit')+1])
for flag in ('--output','--snapshot','--attribution','--velocity-output','--pressure-output','--geometry','--catalogue','--prediction'):
    if flag in args:
        p=pathlib.Path(args[args.index(flag)+1])
        p.write_text(json.dumps({'diagnostic_accepted':code==0 and '--fixture-reject' not in args,'numerically_accepted':code==0,'iterations':3000,'iteration_limit':3000,'final_residual':.1}))
print(json.dumps({'phase':'fixture','iteration':3000}),flush=True)
print('retained diagnostic stderr',file=sys.stderr,flush=True)
sys.exit(code)
"""

class ReferenceSupervision(unittest.TestCase):
    def prepare(self,base,relative):
        spec=importlib.util.spec_from_file_location('reference_fixture',ROOT/relative)
        m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
        repo=base/Path(relative).stem;repo.mkdir();(repo/'scripts').mkdir()
        interpreter=repo/'build/cfd-reference-venv/bin/python';interpreter.parent.mkdir(parents=True)
        interpreter.symlink_to(sys.executable)
        for name in m.SOURCES:
            target=repo/'scripts'/name
            if name=='cfd_run_support.py':shutil.copy2(ROOT/'scripts'/name,target)
            else:target.write_text(PROBE if name==m.SOURCES[0] else '# frozen fixture dependency\n')
        m.ROOT=repo;m.DATA=repo/'data/experiments/reference-fixture/runs'
        return m,repo
    def test_every_family_success_failure_cache_and_frozen_supervisor(self):
        self.assertEqual(len(FAMILIES),55)
        for code in (0,2):
            with tempfile.TemporaryDirectory() as temp:
                base=Path(temp).resolve()
                for relative in FAMILIES:
                    with self.subTest(family=relative,code=code):
                        m,repo=self.prepare(base,relative)
                        with contextlib.redirect_stdout(io.StringIO()):
                            self.assertEqual(m.run('proof',['--fixture-exit',str(code)]),code==0)
                        receipt_path=next(m.DATA.glob('*/proof-receipt.json'))
                        before=receipt_path.read_bytes();receipt=json.loads(before)
                        self.assertEqual(receipt['returncode'],code)
                        self.assertIsNone(receipt['stop_reason'])
                        self.assertEqual(receipt['source_sha256']['cfd_run_support.py'],hashlib.sha256((ROOT/'scripts/cfd_run_support.py').read_bytes()).hexdigest())
                        self.assertFalse(receipt['supervision']['all_external_descendants_verified_terminal'])
                        log=receipt_path.parent/'proof.log'
                        self.assertIn('retained diagnostic stderr',log.read_text())
                        self.assertIn('fixture',log.read_text())
                        with contextlib.redirect_stdout(io.StringIO()):
                            self.assertEqual(m.run('proof',['--fixture-exit',str(code)]),code==0)
                        self.assertEqual(receipt_path.read_bytes(),before)
                        altered=json.loads(before);del altered['artifact_sha256'][str(log)]
                        receipt_path.write_text(json.dumps(altered))
                        with contextlib.redirect_stdout(io.StringIO()),self.assertRaises(ValueError):m.run('proof',['--fixture-exit',str(code)])
                        receipt_path.write_bytes(before)
                        self.assertFalse((receipt_path.parent/'reference.stdout').exists())
    def test_rejected_diagnostic_with_zero_exit_stays_rejected_on_cache_read(self):
        with tempfile.TemporaryDirectory() as temp:
            m,repo=self.prepare(Path(temp).resolve(),'scripts/run_cfd_reference3d_accuracy_signed.py')
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertFalse(m.run('rejected',['--fixture-exit','0','--fixture-reject']))
            p=next(m.DATA.glob('*/rejected-receipt.json'));before=p.read_bytes()
            self.assertEqual(json.loads(before)['diagnostic_failure']['kind'],'missing_diagnostic_acceptance')
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertFalse(m.run('rejected',['--fixture-exit','0','--fixture-reject']))
            self.assertEqual(p.read_bytes(),before)

    def test_invalid_case_names_are_refused_before_allocation(self):
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp).resolve()
            for relative in FAMILIES:
                with self.subTest(family=relative):
                    m,repo=self.prepare(base,relative)
                    for name in ('../escape','a/b','',None):
                        with self.assertRaises(ValueError):m.run(name,[])
                    self.assertFalse(m.DATA.exists())
    def test_cached_receipt_admission_remains_active_under_optimized_python(self):
        with tempfile.TemporaryDirectory() as temp:
            m,repo=self.prepare(Path(temp).resolve(),'scripts/run_cfd_reference3d_quartic.py')
            with contextlib.redirect_stdout(io.StringIO()):self.assertTrue(m.run('proof',['--fixture-exit','0']))
            p=next(m.DATA.glob('*/proof-receipt.json'));receipt=json.loads(p.read_text());receipt['command']=['tampered'];p.write_text(json.dumps(receipt))
            runner="""import importlib.util,sys
from pathlib import Path
spec=importlib.util.spec_from_file_location('reference_fixture',sys.argv[1]);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.ROOT=Path(sys.argv[2]);m.DATA=Path(sys.argv[3])
try:m.run('proof',['--fixture-exit','0'])
except ValueError:raise SystemExit(2)
raise SystemExit(0)
"""
            result=subprocess.run([sys.executable,'-O','-B','-c',runner,str(ROOT/'scripts/run_cfd_reference3d_quartic.py'),str(repo),str(m.DATA)],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,2,result.stdout+result.stderr)
            self.assertEqual(json.loads(p.read_text())['command'],['tampered'])

if __name__=='__main__':unittest.main()
