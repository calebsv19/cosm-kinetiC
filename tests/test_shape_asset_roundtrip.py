import os,subprocess,tempfile,unittest
from pathlib import Path
class AssetRoundtrip(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.binary=Path(os.environ['PHYSICS_SIM_ASSET_ROUNDTRIP_TEST_BIN']).resolve()
 def invoke(self,mode):
  with tempfile.TemporaryDirectory(prefix='physics-asset-roundtrip-',dir='/private/tmp') as td:
   p=Path(td)/'asset.json';p.write_bytes(b'predecessor');row=subprocess.run([str(self.binary),mode,str(p)],capture_output=True,text=True,timeout=5);self.assertEqual(row.returncode,0,row.stderr)
   return row.stdout.strip(),p.read_bytes(),list(Path(td).glob('.headless-sidecar-*.pending'))
 def refused(self,mode):
  value,data,stages=self.invoke(mode);self.assertEqual(value,'0 0 0');self.assertEqual(data,b'predecessor');self.assertEqual(stages,[])
 def test_excess_total_points_refuse_before_replacement(self):self.refused('points')
 def test_excess_paths_refuse_before_replacement(self):self.refused('paths')
 def test_unsupported_schema_refuses_before_replacement(self):self.refused('schema')
 def test_exact_point_bound_publishes_readable_asset(self):
  value,data,stages=self.invoke('boundary');self.assertEqual(value,'1 1 10000');self.assertNotEqual(data,b'predecessor');self.assertEqual(stages,[])
 def test_normal_publication_is_readable(self):self.assertEqual(self.invoke('normal')[0],'1 1 2')
 def test_exact_path_bound_publishes_readable_asset(self):self.assertEqual(self.invoke('pathboundary')[0],'1 1 2')
 def test_missing_paths_pointer_refuses(self):self.refused('nullpaths')
 def test_missing_points_pointer_refuses(self):self.refused('nullpoints')
 def test_nonfinite_native_point_refuses(self):self.refused('nonfinite')
 def test_oversize_name_refuses(self):self.refused('longname')
 def test_escaped_name_at_encoded_token_bound_publishes_readable_asset(self):self.assertEqual(self.invoke('nameboundary')[0],'1 1 2')
 def test_native_default_schema_remains_readable(self):self.assertEqual(self.invoke('defaultschema')[0],'1 1 2')
 def test_escaped_name_over_encoded_token_bound_refuses(self):self.refused('escapedoversize')
 def test_plain_name_at_encoded_token_bound_publishes_readable_asset(self):self.assertEqual(self.invoke('plainboundary')[0],'1 1 2')
 def test_aggregate_point_limit_refuses_across_paths(self):self.refused('aggregate')
 def test_aggregate_point_boundary_is_readable(self):self.assertEqual(self.invoke('aggregateboundary')[0],'1 1 5000')
 def test_max_finite_float_serialization_is_readable(self):self.assertEqual(self.invoke('maxfloat')[0],'1 1 2')
if __name__=='__main__':unittest.main()
