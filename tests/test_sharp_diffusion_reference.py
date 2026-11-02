import math,sys,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from fire_fluid_sharp_diffusion import ALPHA,continuum,semidiscrete

class SharpReference(unittest.TestCase):
    def test_heat_and_interface_transfer(self):
        for length,t in [(1,.0025),(1,3.2),(.0625,.0025),(.03125,.0025)]:
            n=256;a=length/8 if length==1 else length/2
            v=continuum(n,length,a,t);h=length/n
            self.assertAlmostEqual(float(v.sum()*h),a*t,places=12)
            # Semi-infinite source interface integral. Opposite source edge and
            # endpoint images are exponentially remote for these cases.
            expected=2/3*math.sqrt(ALPHA/math.pi)*t**1.5
            observed=float(v[round(a/h):].sum()*h)
            self.assertLess(abs(observed/expected-1),1e-8)
    def test_grid_spectrum_against_symmetric_matrix(self):
        n=32;length=.0625;a=length/2;t=.0025
        matrix=np.diag(np.full(n,-2.))+np.diag(np.ones(n-1),1)+np.diag(np.ones(n-1),-1)
        matrix[0,0]=matrix[-1,-1]=-1;matrix*=ALPHA/(length/n)**2
        eigen,basis=np.linalg.eigh(matrix);gain=np.full(n,t)
        mask=np.abs(eigen)>1e-10;gain[mask]=np.expm1(eigen[mask]*t)/eigen[mask]
        source=(np.arange(n)<n//2).astype(float);expected=basis@(gain*(basis.T@source))
        self.assertLess(float(np.max(np.abs(expected-semidiscrete(n,length,a,t)))),1e-16)
    def test_time_reference_mass(self):
        for n in (32,64,128,256):
            v=semidiscrete(n,1,.125,3.2)
            self.assertAlmostEqual(float(v.sum()/n),.4,places=12)

if __name__=='__main__':unittest.main()
