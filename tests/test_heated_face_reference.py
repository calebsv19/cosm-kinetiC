"""Check the independent multi-cell oracle against closed-form limits."""
import math
import unittest
import numpy as np
from fire_fluid_heated_face_qualification import exact_response


class HeatedFaceReferenceTests(unittest.TestCase):
    def test_uniform_bath_closed_form(self):
        for n in (32,64):
            cells=(n//4)**2
            cg=cells*1206/n**3;cs=100.;g=10/16
            eq=cg*10/(cg+cs)
            expected=300+eq*(1-math.exp(-g*(1/cg+1/cs)*3.2))
            answer=exact_response([310.]*cells,n)
            self.assertAlmostEqual(answer[-1],expected,places=11)
            self.assertAlmostEqual(float(np.ptp(answer[:-1])),0.,places=11)

    def test_equilibrium_is_unchanged(self):
        np.testing.assert_array_equal(exact_response([300.]*64,32),[300.]*65)

    def test_nonuniform_bath_conserves_energy_and_stays_bounded(self):
        initial=np.linspace(300.,315.,64)
        final=exact_response(initial,32)
        before=float(np.sum(initial-300))*1206/32**3
        after=float(np.sum(final[:-1]-300))*1206/32**3+(final[-1]-300)*100
        self.assertAlmostEqual(before,after,places=10)
        self.assertGreaterEqual(float(final.min()),300.)
        self.assertLessEqual(float(final.max()),315.)


if __name__=='__main__':unittest.main()
