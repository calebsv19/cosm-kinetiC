import sys,unittest
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_exact_prediction import encode,decode_chunks
from cfd_reference3d_shared_factor import storage_sha
class ExactPrediction(unittest.TestCase):
    def test_random_supported_coefficients_and_signed_zeros_exact(self):
        rng=np.random.default_rng(911);a=np.r_[0.,-0.,rng.uniform(-1,1,2000)*2.**rng.integers(-90,90,2000)];original=a.view(np.uint64).copy()
        for batch in (1,17,262144):
            p,c,m=encode(a,batch);b=np.concatenate(list(decode_chunks(p,c,31)));np.testing.assert_array_equal(b.view(np.uint64),original);np.testing.assert_array_equal(a.view(np.uint64),original);self.assertEqual(storage_sha(a.astype(np.float32)),m['predictor_float_sha256']);self.assertEqual(m['original_value_sha256'],m['decoded_value_sha256']);self.assertFalse(p.flags.writeable);self.assertFalse(c.flags.writeable)
    def test_float_rounding_boundary_and_joint_accounting(self):
        a=np.array([1+2.**-24+2.**-52,1+2.**-24-2.**-52,-1-2.**-24-2.**-52,np.finfo(np.float32).tiny,0.,-0.]);p,c,m=encode(a,3);b=np.concatenate(list(decode_chunks(p,c,4)));np.testing.assert_array_equal(a.view(np.uint64),b.view(np.uint64));self.assertEqual(m['original_joint_bytes'],12*a.size);self.assertEqual(m['encoded_joint_bytes'],8*a.size);self.assertEqual(m['saved_joint_bytes'],4*a.size);self.assertTrue(np.any(c<0));self.assertTrue(np.any(c>0))
    def test_unsupported_no_partial_precision_fallback(self):
        for value in (np.nan,np.inf,1e300,1e-300,1e-40):
            with self.assertRaises(ValueError):encode(np.array([value]))
        for a in (np.array([]),np.ones(3,dtype=np.float32),np.arange(10.)[::2],np.ones((2,2))):
            with self.assertRaises(ValueError):encode(a)
        with self.assertRaises(ValueError):list(decode_chunks(np.ones(2,dtype=np.float32),np.ones(3,dtype=np.int32)))
    def test_output_and_work_are_guarded_before_allocation(self):
        phases=[];a=np.tile(np.array([1.,2.,3.]),60);p,c,m=encode(a,16,lambda s,n:phases.append((s,n)));self.assertEqual(phases[0],('predictor_output_allocation',8*a.size+64*16));self.assertEqual(m['coefficient_batch_cap'],16);self.assertGreater(m['batches'],1)
        def stop(s,n):raise RuntimeError('budget')
        with self.assertRaisesRegex(RuntimeError,'budget'):encode(a,16,stop)
if __name__=='__main__':unittest.main()
