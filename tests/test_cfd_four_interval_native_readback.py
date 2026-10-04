"""Actual native binary layout and analytic cubic force without a CFD solve."""
import unittest,tempfile,struct,sys
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from assess_cfd_3d_four_interval import native_trace_four
class NativeReadback(unittest.TestCase):
    def test_exact_cubic_and_closed_pressure_gauge_in_native_layout(self):
        with tempfile.TemporaryDirectory() as folder:
            for n in (16,32):
                h=2/n;a=np.arange(2*n)*h;b=a+h
                p=(((b**4-a**4)/(4*h))+3.2)[None,None,:]
                values=np.zeros((n,n,2*n,4));values[:,:,:,3]=p
                binary=Path(folder)/f'n{n}.bin'
                binary.write_bytes(struct.pack('=3i',2*n,n,n)+values.tobytes()+np.zeros(n*n).tobytes())
                force,averages=native_trace_four(binary,n)
                self.assertAlmostEqual(force,1.5**3-2.5**3,places=11)
                self.assertEqual(len(averages[0]),4)
                values[:,:,:,3]-=3.2
                binary.write_bytes(struct.pack('=3i',2*n,n,n)+values.tobytes()+np.zeros(n*n).tobytes())
                self.assertAlmostEqual(native_trace_four(binary,n)[0],force,places=11)
    def test_missing_outlet_tail_and_extra_bytes_reject(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'bad.bin';n=16;data=struct.pack('=3i',2*n,n,n)+np.zeros(2*n**3*4).tobytes()
            p.write_bytes(data)
            with self.assertRaises(ValueError):native_trace_four(p,n)
            p.write_bytes(data+np.zeros(n*n).tobytes()+b'x')
            with self.assertRaises(ValueError):native_trace_four(p,n)
if __name__=='__main__':unittest.main()
