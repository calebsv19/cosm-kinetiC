"""Explicit saved-experiment checks; excluded from current source fixtures."""
import unittest
import test_cfd_reference3d_selective_mesh as current
from cfd_reference_test_support import archive_root
globals().update({k:v for k,v in vars(current).items() if not k.startswith('__') and not (isinstance(v,type) and issubclass(v,unittest.TestCase))})
import os
value=os.environ.get('PHYSICS_SIM_SELECTIVE_DIAGNOSTIC')
if not value:
    raise ValueError('Set PHYSICS_SIM_SELECTIVE_DIAGNOSTIC to the exact saved diagnostic')
DIAGNOSTIC=Path(value).resolve(strict=True)

class SavedExperiment(unittest.TestCase):
    def test_mirror_true_boundaries_volume_and_fixed_body_for_all_candidates(self):
        original,lo,hi,axes,_=domain_mesh(8.,4,True,mode='held_l4')
        body_points=lambda m: {tuple(p) for p in np.round(m.p[:,np.unique(m.facets[:,m.boundaries['body']])].T,12)}
        expected_body=body_points(original)
        for marks,count in ((1,14016),(2,14208),(4,14400),(8,15168)):
            (m,l,h,a,macros),meta=selective_mesh(DIAGNOSTIC,marks)
            self.assertEqual(m.nelements,count);self.assertEqual(m.nelements,4*macros)
            self.assertAlmostEqual(np.abs(m.mapping().detA).sum()/6,31.,places=10)
            self.assertEqual(body_points(m),expected_body)
            for old,new in zip(axes,a):np.testing.assert_array_equal(old,new)
            points={tuple(x) for x in np.round(m.p.T,11)}
            for axis,L in enumerate((8.,2.,2.)):
                reflected=m.p.copy();reflected[axis]=L-reflected[axis]
                self.assertEqual(points,{tuple(x) for x in np.round(reflected.T,11)})
            for name,faces in m.boundaries.items():
                v=m.p[:,m.facets[:,faces]]
                area=np.linalg.norm(np.cross((v[:,1]-v[:,0]).T,(v[:,2]-v[:,0]).T),axis=1).sum()/2
                self.assertAlmostEqual(area,{'body':6.,'inlet':4.,'outlet':4.,'walls':64.}[name],places=10)
            self.assertTrue(meta['body_facets_unchanged']);self.assertLess(meta['macro_center_match_max_m'],1e-11)
            self.assertTrue(all(c[0]<axes[0][1] for c in meta['selected_octant_centers_m']))

    def test_alfield_centers_and_paired_yz_marking(self):
        (m,*_),meta=selective_mesh(DIAGNOSTIC,2)
        selected={tuple(np.round(c,10)) for c in meta['selected_octant_centers_m']}
        self.assertEqual(selected,{(x,z,y) for x,y,z in selected})
        # Check all parents, rather than only a handpicked marked parent.
        nmacro=m.nelements//4
        for i in range(nmacro):
            children=m.t[:,i+np.arange(4)*nmacro];ids=np.unique(children);center=ids[-1];corners=ids[:-1]
            self.assertEqual(len(corners),4)
            np.testing.assert_allclose(m.p[:,center],m.p[:,corners].mean(axis=1),rtol=0,atol=1e-12)

    def test_unsupported_and_tampered_observation_are_rejected(self):
        with self.assertRaises(ValueError):selective_mesh(DIAGNOSTIC,3)
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/DIAGNOSTIC.name
            row=json.loads(DIAGNOSTIC.read_text());row['equilibrium_indicator_squared_per_tet'][0]+=1
            target.write_text(json.dumps(row))
            receipt=DIAGNOSTIC.with_name(DIAGNOSTIC.stem+'-receipt.json')
            target.with_name(target.stem+'-receipt.json').write_bytes(receipt.read_bytes())
            with self.assertRaises((AssertionError,KeyError)):selective_mesh(target,2)

if __name__=='__main__':unittest.main()
