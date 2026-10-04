"""Exact predecessor reconstruction, symmetry and rejected input drift."""
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_spatial_mesh import spatial_mesh
from cfd_reference3d_preconditioner import array_sha
from cfd_reference3d_adaptive_mesh import adaptive_mesh


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def fixture(root):
    mesh,lo,hi,axes,_=spatial_mesh(count=6,normal_spacing=.03,insert_normal=True)
    snapshot=root/'field.npz';np.savez_compressed(snapshot,vertices_m=mesh.p,tetrahedra=mesh.t,lo=lo,hi=hi)
    original=root/'field.json';original.write_text(json.dumps({'numerically_accepted':True,'body':True,'count':6,
        'split_first_normal':False,'length':4.,'axis_nodes_m':[a.tolist() for a in axes],
        'identity':{'mesh_sha256':array_sha(mesh.p,mesh.t)}}))
    receipt=root/'field-receipt.json';receipt.write_text(json.dumps({'artifact_sha256':{str(p):sha(p) for p in (snapshot,original)}}))
    diagnostic=root/'diagnostic.json';diagnostic.write_text(json.dumps({'diagnostic_accepted':True,
        'input_receipt':str(receipt),'input_receipt_sha256':sha(receipt),'input_snapshot':str(snapshot),
        'input_snapshot_sha256':sha(snapshot),'equilibrium_indicator_squared_per_tet':np.ones(mesh.nelements).tolist()}))
    return diagnostic,snapshot,mesh


class AdaptiveMesh(unittest.TestCase):
    def test_geometry_and_full_reflection_symmetry(self):
        with tempfile.TemporaryDirectory() as d:
            diagnostic,snapshot,base=fixture(Path(d));before=sha(snapshot)
            data,meta=adaptive_mesh(diagnostic,32);mesh,lo,hi,axes,parents=data
            self.assertGreater(mesh.nelements,base.nelements);self.assertLess(mesh.nelements,50000)
            self.assertEqual(mesh.nelements,4*parents)
            self.assertAlmostEqual(np.abs(mesh.mapping().detA).sum()/6,15.,places=10)
            vertices={tuple(p) for p in np.round(mesh.p.T,11)}
            for axis,L in enumerate((4.,2.,2.)):
                reflected=mesh.p.copy();reflected[axis]=L-reflected[axis]
                self.assertEqual(vertices,{tuple(p) for p in np.round(reflected.T,11)})
            body=mesh.p[:,mesh.facets[:,mesh.boundaries['body']]]
            area=np.linalg.norm(np.cross((body[:,1]-body[:,0]).T,(body[:,2]-body[:,0]).T),axis=1).sum()/2
            self.assertAlmostEqual(area,6.,places=10)
            self.assertEqual(before,sha(snapshot))
            self.assertAlmostEqual(meta['captured_indicator_fraction'],32/738,places=12)

    def test_changed_input_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            diagnostic,snapshot,_=fixture(Path(d));snapshot.write_bytes(snapshot.read_bytes()+b'changed')
            with self.assertRaises(AssertionError):adaptive_mesh(diagnostic,8)


if __name__=='__main__':unittest.main()
