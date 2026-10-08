"""Reference policy admission and guaranteed no-codec small original preservation."""
import sys,unittest
from pathlib import Path
from unittest.mock import patch
import numpy as np
from scipy.sparse import csr_matrix
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
from cfd_reference3d_size_selected import select,decision,THRESHOLD_BYTES
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_shared_factor import storage_sha
LIB=library_path('build/c3d-encoded-operator/support/factor.dylib')
class Selected(unittest.TestCase):
 def test_below_threshold_original_identity_action_and_no_encoding_or_guard(self):
  t=VectorTriangle(csr_matrix(np.eye(6)),LIB);h=t.input_sha256;x=np.arange(6.)
  with patch('cfd_reference3d_size_selected.EncodedTriangle',side_effect=AssertionError('small path encoded')):
   v,c=select(t,LIB,check=lambda *a:(_ for _ in ()).throw(AssertionError('small encoding guard called')))
  self.assertIs(v,t);self.assertFalse(c['encoded_selected']);self.assertEqual(v.input_sha256,h);np.testing.assert_array_equal(v@x,x)
  t.values[0]=2.
  with self.assertRaises(ValueError):select(t,LIB)
 def test_boundary_calibration_override_and_invalid_requests(self):
  limit=THRESHOLD_BYTES//4
  self.assertFalse(decision(limit-1)['encoded_selected']);self.assertTrue(decision(limit)['encoded_selected']);self.assertTrue(decision(22411620)['encoded_selected']);self.assertFalse(decision(22411620,'legacy')['encoded_selected'])
  for n,mode in ((0,'auto'),(True,'auto'),(50000001,'auto'),(limit,'force'),(3.5,'auto')):
   with self.assertRaises(ValueError):decision(n,mode)
  with self.assertRaises(ValueError):select(np.eye(6),LIB)
if __name__=='__main__':unittest.main()
