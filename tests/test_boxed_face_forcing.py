import math
import unittest
from unittest.mock import patch
import numpy as np
from coupling.boxed_face_forcing import patches,response,relative,RHO_CP
from coupling.boxed_face_forcing import report
from fire_fluid_heated_face_qualification import exact_response


class FixedSupportTests(unittest.TestCase):
    def test_mixed_scalar_operators_cannot_qualify_spatial_refinement(self):
        cases=[{'identity':{'scalar_reconstruction':'minmod'}},
               {'identity':{'scalar_reconstruction':'mc'}}]
        with patch('coupling.boxed_face_forcing.load',side_effect=cases):
            with self.assertRaisesRegex(ValueError,'same scalar operator'):report('unused')

    def test_common_patch_geometry_affine(self):
        expected=np.array([2+(12+i+.5)/32+2*(12+j+.5)/32+3*11.5/32
                           for j in range(8) for i in range(8)])
        for n in (32,64):
            z,y,x=np.indices((n,n,n))
            e=RHO_CP/n**3*(2+(x+.5)/n+2*(y+.5)/n+3*(z+.5)/n)
            np.testing.assert_allclose(patches(e,n),expected,atol=1e-13,rtol=0)

    def test_finite_bath_unchanged_by_partition(self):
        answers=[]
        for n in (32,64):
            cells=round((n/4)**2*n/32)
            answers.append(exact_response([310.]*cells,n,depth_m=1/32)[-1])
        self.assertAlmostEqual(*answers,places=11)

    def test_prescribed_response_cadence_on_constant_field(self):
        rows=[{'support_mean_rise_k':10.} for _ in range(129)]
        exact=10*(-math.expm1(-.625*3.2/100))
        for count,cadences in ((129,(.1,.05,.025)),(641,(.1,.05,.025,.01,.005))):
            rows=[{'support_mean_rise_k':10.} for _ in range(count)]
            for dt in cadences:
                d=response(rows,dt)
                self.assertAlmostEqual(d['surface_rise_k'],exact,places=13)
                self.assertAlmostEqual(d['gas_transfer_demand_j'],100*exact,places=11)

    def test_cadence_cannot_invent_missing_observations(self):
        rows=[{'support_mean_rise_k':10.} for _ in range(129)]
        for dt in (.005,.01,.0125,.03):
            with self.assertRaises(ValueError):response(rows,dt)

    def test_zero_signal_is_not_a_convergence_pass(self):
        self.assertIsNone(relative([0.],[0.]))
        self.assertEqual(relative([2.],[1.]),1.)
        with self.assertRaises(ValueError):relative([float('nan')],[1.])
        with self.assertRaises(ValueError):relative([1,2],[1])

    def test_unaligned_depth_rejects(self):
        with self.assertRaises(ValueError):exact_response([310.]*64,32,depth_m=.031)


if __name__=='__main__':unittest.main()
