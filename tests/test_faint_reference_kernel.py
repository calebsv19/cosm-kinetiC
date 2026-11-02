import math
from pathlib import Path
import sys
import unittest
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from fire_fluid_faint_kernel import ALPHA, H, arrival, erfcx, free_shear_tail


class KernelChecks(unittest.TestCase):
    def test_scaled_erfc_transition(self):
        for v in [0., 1., 10., 19.999, 20., 22., 26.]:
            self.assertLess(abs(float(erfcx(v)) /
                                (math.exp(v*v)*math.erfc(v)) - 1), 2e-14)

    def test_robin_kernel_integral(self):
        # A uniform initial half-line at unit temperature has boundary
        # temperature erfcx(kappa*sqrt(alpha*t)). Test Green normalization.
        x, w = np.polynomial.legendre.leggauss(128)
        for t in [.025, .1, .5]:
            root = math.sqrt(ALPHA*t)
            z = (x+1)*6*root
            a = z/(2*root)
            kappa = H/.026
            g = np.exp(-a*a)*(1/(math.sqrt(math.pi)*root)
                               - kappa*erfcx(a+kappa*root))
            value = float(np.dot(w*6*root, g))
            self.assertLess(abs(value/float(erfcx(kappa*root))-1), 1e-12)

    def test_faint_arrival_quadrature_and_bounds(self):
        previous = 0.
        for t in [.025, .1, .4, .5]:
            a, b = arrival(t, 128), arrival(t, 256)
            self.assertLess(abs(a['cold_q_j']/b['cold_q_j']-1), 1e-10)
            self.assertGreater(b['lower_q_j'], previous)
            self.assertLessEqual(b['lower_q_j'], b['upper_q_j'])
            self.assertLess(b['feedback_bound_j']/b['cold_q_j'], .0005)
            self.assertLess(b['top_image_bound_j']/b['cold_q_j'], 1e-80)
            previous = b['upper_q_j']
        self.assertEqual(arrival(0)['cold_q_j'], 0)

    def test_positive_shear_tail_quadrature(self):
        self.assertEqual(free_shear_tail(0), 0)
        a, b = free_shear_tail(.1, 48), free_shear_tail(.1, 64)
        self.assertGreater(b, 0)
        self.assertLess(abs(a/b-1), 1e-9)


if __name__ == '__main__':
    unittest.main()
