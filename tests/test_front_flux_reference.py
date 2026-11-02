"""Independent differential/integral checks on the frozen face oracle."""
import sys,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from fire_fluid_front_flux import point_reference,reference

class FrontFluxReference(unittest.TestCase):
    def test_gradient_against_temperature_difference(self):
        x=np.array([0,.03125,.0625,.09375,.125,.21875])
        for t in [.5,.75,1.5,3.2]:
            for u in [0,.003125,.01875]:
                _,g=point_reference(x,t,u)
                plus,_=point_reference(x+1e-7,t,u);minus,_=point_reference(x-1e-7,t,u)
                np.testing.assert_allclose((plus-minus)/2e-7,g,rtol=1e-6,atol=1e-13)

    def test_cell_energy_rate_matches_face_flux(self):
        n=128;h=.25/n;x=np.arange(n)*h;epsilon=1e-5
        for t in [.5,.75,1.5,3.2]:
            for u in [0,.003125,.01875]:
                temp,g=point_reference(x,t,u);flux=u*temp-.026/1206*g
                divergence=(flux-np.roll(flux,-1))/h
                temporal=(reference(n,t+epsilon,u,.026,32)-reference(n,t-epsilon,u,.026,32))/(2*epsilon)
                for patch in [0,2,3,4,7]:
                    sl=slice(patch*16,(patch+1)*16)
                    relative=float(abs(temporal[sl]-divergence[sl]).sum()/abs(divergence[sl]).sum())
                    self.assertLess(relative,1e-5)

if __name__=='__main__':unittest.main()
