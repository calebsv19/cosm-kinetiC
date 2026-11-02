"""Independent oracle invariants for the bounded sharp-front investigation."""
import sys,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from fire_fluid_front_arrival import reference

class FrontReference(unittest.TestCase):
    def test_initial_and_pure_advection_cell_shift(self):
        n=128;v=reference(n,0,0,.026)
        self.assertEqual(float(v.sum()*.25/n),1/32)
        shifted=reference(n,(.25/n)/.01875,.01875,0)
        np.testing.assert_allclose(shifted,np.roll(v,1),rtol=0,atol=2e-14)

    def test_positive_bounded_conservative_kernel(self):
        for n in [128,256]:
            for t in [.025,.5,3.2]:
                for u in [-.01875,0,.003125,.01875]:
                    v=reference(n,t,u,.026)
                    self.assertGreaterEqual(float(v.min()),0)
                    self.assertLessEqual(float(v.max()),1+1e-14)
                    self.assertAlmostEqual(float(v.sum()*.25/n),1/32,places=14)

    def test_reflection_and_diffusive_translation(self):
        n=128;indices=(47-np.arange(n))%n
        for t in [.025,1.5,3.2]:
            np.testing.assert_allclose(reference(n,t,.01875,.026),reference(n,t,-.01875,.026)[indices],rtol=2e-12,atol=1e-16)
        t=(.25/n)/.01875
        np.testing.assert_allclose(reference(n,t,.01875,.026),np.roll(reference(n,t,0,.026),1),rtol=2e-12,atol=1e-16)

    def test_weak_tail_quadrature(self):
        for t in [.025,1.5,3.2]:
            a=reference(128,t,.01875,.026,16);b=reference(128,t,.01875,.026,32)
            # Apply the declared 0.5% oracle refinement criterion even to
            # individual nearly-underflowing cells; no absolute-value waiver.
            np.testing.assert_allclose(a,b,rtol=.005,atol=0)

if __name__=='__main__':unittest.main()
