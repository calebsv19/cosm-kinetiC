"""Immutable input, force score aggregation and prospective geometry refusal."""
import sys,json,unittest,tempfile,shutil
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_force_local_mesh import force_ranked_input,force_local_mesh,LocalGeometryRejected
from cfd_reference3d_adaptive_mesh import mirror
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_corner_local_mesh import shape_inverse
from cfd_reference3d_mesh import edge_distance
DIAGNOSTIC=next((ROOT/'build/c3d-end-plateau/observer-runs').glob('*/L8-body6-normal-held-outer2-signed-force.json'))

class ForceLocal(unittest.TestCase):
    def test_signed_macro_aggregation_and_actual_symmetric_pairs(self):
        saved,octant,lo,hi,axes,lengths,scores,pairs,binding=force_ranked_input(DIAGNOSTIC)
        centers=octant.p[:,octant.t].mean(axis=1).T
        for pair in pairs[:8]:
            self.assertEqual(set(map(tuple,np.round(centers[list(pair)],11))),set(map(tuple,np.round(centers[list(pair)][:,[0,2,1]],11))))
            self.assertTrue(np.all(edge_distance(centers[list(pair)].T,lo,hi)<.15))
        diagnostic=json.loads(DIAGNOSTIC.read_text())
        with np.load(diagnostic['signed_attribution_path'],allow_pickle=False) as s:
            total=s['cell_net_n'].sum(axis=1)[:,0]
        # Independent parent grouping by each child's actual barycenter ID.
        parents=saved.t.max(axis=0);unique=np.unique(parents)
        actual=np.stack([[total[lift,parents==parent].sum() for parent in unique] for lift in range(2)])
        grouped=np.max(np.abs(actual),axis=0)
        self.assertAlmostEqual(float(scores.sum()),float(grouped.sum()),places=12)
        self.assertGreater(scores[list(pairs[0])].sum(),0)
        self.assertLessEqual(saved.nelements,50000)

    def test_actual_pair_rejection_matches_intrinsic_shape(self):
        old,octant,lo,hi,axes,lengths,scores,pairs,binding=force_ranked_input(DIAGNOSTIC)
        with self.assertRaises(LocalGeometryRejected) as caught:force_local_mesh(DIAGNOSTIC,0,'bisection')
        meta=caught.exception.metadata
        self.assertIn('affected intrinsic worst shape worsened',caught.exception.reasons)
        self.assertGreater(meta['refined_affected_worst_shape'],meta['original_affected_worst_shape'])
        mesh=alfeld_split(mirror(octant.refined(np.array(pairs[0])),lengths))
        self.assertEqual(mesh.nelements,meta['refined_tetrahedra'])
        self.assertAlmostEqual(np.abs(mesh.mapping().detA).sum()/6,31.,places=10)
        self.assertGreater(np.abs(mesh.mapping().detA).min(),0)
        self.assertTrue(np.all(np.isfinite(shape_inverse(mesh))))
        np.testing.assert_allclose(list(meta['boundary_areas_m2'].values()),[4.,4.,64.,6.],rtol=0,atol=1e-9)

    def test_unbound_or_tampered_diagnostic_refused_before_ranking(self):
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/DIAGNOSTIC.name
            target.write_bytes(DIAGNOSTIC.read_bytes());receipt=DIAGNOSTIC.with_name(DIAGNOSTIC.stem+'-receipt.json')
            shutil.copy2(receipt,target.with_name(target.stem+'-receipt.json'))
            shutil.copytree(receipt.parent/'source',Path(directory)/'source')
            with self.assertRaises((AssertionError,KeyError)):force_ranked_input(target)
            row=json.loads(target.read_text());row['signed_force_attribution']['maximum_score_n']+=1;target.write_text(json.dumps(row))
            with self.assertRaises((AssertionError,KeyError)):force_ranked_input(target)
        with self.assertRaises(ValueError):force_local_mesh(DIAGNOSTIC,8)

if __name__=='__main__':unittest.main()
