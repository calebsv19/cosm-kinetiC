#!/usr/bin/env python3
"""Full accepted saved-mesh force comparisons; retain separate domain/refinement gates."""
import json,hashlib
from pathlib import Path
import numpy as np
from skfem import MeshTet
from audit_cfd_3d_spatial import verify_receipt,force_comparison
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-retained-margin'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
 r,a=verify_receipt(p);assert r['returncode']==0 and a['final_residual']['true_residual']<1e-10
 snap=Path(r['command'][r['command'].index('--snapshot')+1]);return r,a,snap

def main():
 out=D/'force-comparisons.json';assert not out.exists()
 p=next((D/'runs').glob('*/L8-*-receipt.json'));r,a,snap=read(p)
 bp=next((R/'build/c3d-second-normal/runs').glob('*/L4-body6-second-normal-tensor-receipt.json'));br,b,bsnap=read(bp)
 inputs={str(q):sha(q) for q in (p,snap,bp,bsnap)}
 with np.load(snap,allow_pickle=False) as z: v=z['vertices_m'];t=z['tetrahedra'];lo=z['lo'];hi=z['hi']
 with np.load(bsnap,allow_pickle=False) as z: u=z['vertices_m'];s=z['tetrahedra'];ulo=z['lo'];uhi=z['hi']
 u=u.copy();u[0]+=2;np.testing.assert_allclose(lo,ulo+[2,0,0],rtol=0,atol=1e-14);np.testing.assert_allclose(hi,uhi+[2,0,0],rtol=0,atol=1e-14)
 def inner(points,cells):
  v=points[:,cells];keep=np.all((v[0]>=3.3125-1e-11)&(v[0]<=4.6875+1e-11),axis=0)
  return {tuple(sorted(map(tuple,np.round(points[:,t].T,11)))) for t in cells[:,keep].T}
 ka,kb=inner(u,s),inner(v,t);assert ka==kb and len(ka)==23616
 def body(points,cells):
  m=MeshTet(points,cells);f=m.boundary_facets();x=points[:,m.facets[:,f]].mean(axis=1)
  keep=np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
  return {tuple(sorted(map(tuple,np.round(points[:,m.facets[:,f0]].T,11)))) for f0 in f[keep]}
 assert body(u,s)==body(v,t)
 comparisons={str(p):dict(base_receipt=str(bp),purpose='domain sensitivity with translated cube surface and complete inner cells held',force_comparison=force_comparison(a,b))}
 oldp=next((R/'build/c3d-mixed-precision/runs').glob('*/L8-body6-normal-held-outer2-mixed-receipt.json'));orr,old,oldnpz=read(oldp);inputs.update({str(oldp):sha(oldp),str(oldnpz):sha(oldnpz)})
 refinement=dict(base_receipt=str(oldp),purpose='streamwise normal resolution on the same L8 domain; distinct declared non-nested tensor remesh',force_comparison=force_comparison(a,old))
 for q,h in inputs.items():assert sha(Path(q))==h
 result=dict(input_sha256=inputs,matched_force_comparisons=comparisons,L8_normal_refinement=refinement,held_inner_tetrahedra=len(ka),saved_cube_surface_triangles_preserved=True,physical_accuracy_certified=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
