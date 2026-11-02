"""Time-integrated face budgets against independent cell-average inventory."""
import sys,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from fire_fluid_front_continuation import integrated_reference
from fire_fluid_front_arrival import reference

class FrontContinuationReference(unittest.TestCase):
    def test_integrated_flux_matches_inventory(self):
        final=reference(128,1.5,.003125,.026,64)
        for start in [0,.025,.1,.5]:
            initial=reference(128,start,.003125,.026,64)
            flux=integrated_reference(start,1.5,64)['total']
            for patch in [0,2,3,4,7]:
                sl=slice(patch*16,(patch+1)*16);west=patch*16;east=((patch+1)*16)%128
                change=(final[sl]-initial[sl]).sum()*.25/128
                self.assertLess(abs((flux[west]-flux[east])/change-1),1e-10)

    def test_integral_additivity_and_components(self):
        whole=integrated_reference(.025,1.5,64)
        first=integrated_reference(.025,.5,64);second=integrated_reference(.5,1.5,64)
        for key in ['total','advection','diffusion']:
            # Selected physical patch faces, including the weak far tail.
            for i in [0,16,32,48,64,80,112]:
                self.assertLess(abs((first[key][i]+second[key][i])/whole[key][i]-1),1e-10)
        np.testing.assert_array_equal(whole['total'],whole['advection']+whole['diffusion'])

if __name__=='__main__':unittest.main()
