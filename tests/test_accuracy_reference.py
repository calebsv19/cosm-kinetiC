import math,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.accuracy_reference import scalar_cell_means,reservoir
class AccuracyReference(unittest.TestCase):
    def test_cell_average_matches_independent_midpoint_quadrature(self):
        for n in (16,32):
            means=scalar_cell_means(n,.8,.05,.026/1206)
            for i in (0,5,n-1):
                samples=[1+.25*math.exp(-.026/1206*(2*math.pi)**2*.8)*math.cos(2*math.pi*((i+(j+.5)/2000)/n-.25-.05*.8)) for j in range(2000)]
                self.assertAlmostEqual(means[i],math.fsum(samples)/2000,places=9)
    def test_scalar_period_and_mean(self):
        a=scalar_cell_means(32,0,.05,0);b=scalar_cell_means(32,20,.05,0)
        self.assertLess(max(abs(x-y) for x,y in zip(a,b)),1e-14);self.assertAlmostEqual(math.fsum(a)/32,1,places=14)
    def test_reservoir_conservation_equilibrium_and_signed_direction(self):
        for g,o in [(5,0),(0,5),(2,2)]:
            a,b=reservoir(3.2,g,o,9.421875,10,.125)
            self.assertAlmostEqual(a*9.421875+b*10,g*9.421875+o*10,places=13)
            self.assertTrue(min(g,o)<=a<=max(g,o));self.assertTrue(min(g,o)<=b<=max(g,o))
            self.assertLessEqual(abs(a-b),abs(g-o))
        self.assertEqual(reservoir(3.2,5,0,9.421875,10,0),(5,0))
    def test_reference_refusals(self):
        for args in [(-1,5,0,10,10,1),(1,5,0,0,10,1),(1,5,0,10,10,-1)]:
            with self.assertRaises(ValueError):reservoir(*args)
if __name__=='__main__':unittest.main()
