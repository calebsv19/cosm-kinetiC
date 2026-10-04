"""Uniform cubical macro duct with original Alfeld children and no body boundary."""
import numpy as np
from cfd_fem_reference3d_solenoidal import mesh_for_case

def empty_duct_mesh(length,count,split,exponent,outer_layers,mode):
 if length not in (4.,8.) or count not in (4,6) or split or exponent!=3 or outer_layers!=1 or mode!='original':raise ValueError('unsupported declared empty duct mesh')
 bundle=mesh_for_case(length,body=False,n=count);mesh,lo,hi,axes,macros=bundle
 assert not len(mesh.boundaries['body']) and mesh.nelements==24*int(length*count/2)*count**2
 assert np.abs(mesh.mapping().detA).min()>0 and abs(np.abs(mesh.mapping().detA).sum()/6-4*length)<1e-9
 for a in axes:np.testing.assert_allclose(np.diff(a),2/count,rtol=2e-13,atol=2e-13)
 return bundle
