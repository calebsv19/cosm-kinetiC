import unittest
import numpy as np
from fire_fluid_transport_report import average
from fire_fluid_trace_diagnosis import crossing

class TransportReferenceTests(unittest.TestCase):
    def test_compact_pulse_mass_and_exact_zero_before_arrival(self):
        z=np.arange(256)/256
        for t in (0.,1.,3.2):
            a=average(z,z+1/256,t,0,.0625)
            self.assertAlmostEqual(a.sum()/256,.75*.0625,places=13)
        self.assertAlmostEqual(float(average(11/32,12/32,1.8,0,.0625)),0.,places=13)
    def test_diffusive_reference_preserves_mass_and_refines_quadrature(self):
        z=np.arange(128)/128
        a=average(z,z+1/128,3.2,.026,.0625,96)
        b=average(z,z+1/128,3.2,.026,.0625,160)
        self.assertAlmostEqual(a.sum()/128,.75*.0625,places=12)
        np.testing.assert_allclose(a,b,atol=1e-12,rtol=1e-10)
    def test_log_interpolated_crossing_and_unreached_level(self):
        t=np.array([0.,1.,2.]);y=np.array([1.,10.,100.])
        self.assertAlmostEqual(crossing(t,y,np.sqrt(1000)),1.5)
        self.assertIsNone(crossing(t,y,101))
if __name__=='__main__':unittest.main()
