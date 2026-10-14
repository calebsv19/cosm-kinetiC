import json,os,shutil,tempfile,unittest
from pathlib import Path
from fire_fluid_momentum_readback import inspect

@unittest.skipUnless(os.environ.get('MOMENTUM_READBACK_FIXTURE'),'requires native capture fixture')
class ReadbackTests(unittest.TestCase):
 def fixture(self,root):
  source=os.environ['MOMENTUM_READBACK_FIXTURE'];prefix=Path(root)/'state'
  for suffix in ['.accepted.bin','.source.bin','.candidate.bin','.rejection.json']:shutil.copyfile(source+suffix,str(prefix)+suffix)
  return str(prefix)
 def test_independent_cfl_and_accepted_accounting(self):
  r=inspect(os.environ['MOMENTUM_READBACK_FIXTURE'])
  self.assertTrue(r['accepted_state_valid']);self.assertGreater(r['independently_recomputed_courant'],.4)
 def test_truncated_accepted_payload_refused(self):
  with tempfile.TemporaryDirectory() as root:
   p=self.fixture(root);q=Path(p+'.accepted.bin');q.write_bytes(q.read_bytes()[:-8])
   with self.assertRaises(ValueError):inspect(p)
 def test_metadata_counters_cannot_alias_state(self):
  with tempfile.TemporaryDirectory() as root:
   p=self.fixture(root);q=Path(p+'.rejection.json');d=json.loads(q.read_text());d['accepted_steps']+=1;q.write_text(json.dumps(d))
   with self.assertRaises(ValueError):inspect(p)
if __name__=='__main__':unittest.main()
