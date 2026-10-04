import sys,unittest
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_coefficient_catalogue import encode,decode_chunks
class Catalogue(unittest.TestCase):
    def test_arbitrary_words_signed_zero_and_nan_payloads(self):
        words=np.r_[np.array([0,2**63,0x7ff8000000000001,0xfff8000000000023,0x7ff0000000000000,1],dtype=np.uint64),np.random.default_rng(122).integers(0,2**64,size=400,dtype=np.uint64)];a=np.tile(words,4).view(np.float64);original=a.view(np.uint64).copy()
        c,ids,m=encode(a,17);b=np.concatenate(list(decode_chunks(c,ids,31)));np.testing.assert_array_equal(b.view(np.uint64),original);np.testing.assert_array_equal(a.view(np.uint64),original);self.assertEqual(m['original_value_sha256'],m['decoded_value_sha256']);self.assertIn(0,c);self.assertIn(2**63,c);self.assertFalse(c.flags.writeable);self.assertFalse(ids.flags.writeable)
    def test_chunk_independent_coefficients_order_and_byte_accounting(self):
        a=np.tile(np.array([3.,-0.,0.,1e-230,-4.1,3.]),300)
        for batch in (1,7,128,262144):
            c,ids,m=encode(a,batch);b=np.concatenate(list(decode_chunks(c,ids,13)));np.testing.assert_array_equal(a.view(np.uint64),b.view(np.uint64));self.assertEqual(m['encoded_bytes'],c.nbytes+ids.nbytes);self.assertEqual(m['saved_bytes'],a.nbytes-c.nbytes-ids.nbytes);self.assertEqual(m['catalogue_words'],5)
    def test_guard_precedes_large_allocations_and_batches_are_bounded(self):
        stages=[];a=np.tile(np.arange(7.),50);c,ids,m=encode(a,16,lambda phase,n:stages.append((phase,n)));self.assertEqual(stages[0],('catalogue_ids_allocation',a.size*4+24*16));self.assertLessEqual(m['maximum_batch_unique_words'],16);self.assertGreater(m['batches'],1)
        def refuse(phase,n):raise RuntimeError('budget')
        with self.assertRaisesRegex(RuntimeError,'budget'):encode(a,16,refuse)
    def test_invalid_codec_and_external_ids_refused(self):
        for a in (np.array([]),np.arange(3,dtype=np.float32),np.arange(10.)[::2],np.ones((2,2))):
            with self.assertRaises(ValueError):encode(a)
        with self.assertRaises(ValueError):encode(np.ones(2),0)
        with self.assertRaises(ValueError):list(decode_chunks(np.array([0],dtype=np.uint64),np.array([1],dtype=np.uint32)))
if __name__=='__main__':unittest.main()
