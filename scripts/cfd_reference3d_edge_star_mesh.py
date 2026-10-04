"""Parent-certified complete-edge-star macro splits; no old Alfeld nesting claim."""
from itertools import combinations
import numpy as np
from skfem import MeshTet
from scipy.spatial import cKDTree
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_adaptive_mesh import mirror
from cfd_reference3d_corner_local_mesh import shape_inverse,LocalGeometryRejected
from cfd_reference3d_mesh import edge_distance

def split_edges(mesh,edges):
 edges=sorted(set(tuple(sorted(map(int,e))) for e in edges))
 original={tuple(sorted(e)) for t in mesh.t.T for e in combinations(t,2)}
 if not edges or any(len(e)!=2 or e not in original for e in edges):raise ValueError('invalid original mesh edges')
 edges=sorted(edges,key=lambda e:(-float(np.sum((mesh.p[:,e[0]]-mesh.p[:,e[1]])**2)),e))
 points=mesh.p.copy();cells=mesh.t.copy();parents=np.arange(mesh.nelements)
 for a,b in edges:
  affected=np.flatnonzero(np.any(cells==a,axis=0)&np.any(cells==b,axis=0))
  assert len(affected)>0
  midpoint=points.shape[1];points=np.column_stack((points,(points[:,a]+points[:,b])/2))
  left=cells[:,affected].copy();right=left.copy();left[left==b]=midpoint;right[right==a]=midpoint
  cells[:,affected]=left;cells=np.column_stack((cells,right));parents=np.r_[parents,parents[affected]]
 refined=MeshTet(points,np.sort(cells,axis=0))
 # Every new cell lies in its actual original parent, and fills it exactly.
 old_volume=np.abs(mesh.mapping().detA)/6;new_volume=np.abs(refined.mapping().detA)/6
 np.testing.assert_allclose(np.bincount(parents,weights=new_volume,minlength=mesh.nelements),old_volume,rtol=2e-12,atol=1e-14)
 assert np.all(new_volume>0)
 vertices=mesh.p[:,mesh.t[:,parents]];A=vertices[:,1:]-vertices[:,:1];rhs=refined.p[:,refined.t]-vertices[:,:1]
 bary=np.linalg.solve(A.transpose(2,0,1),rhs.transpose(2,0,1));bary=np.concatenate((1-bary.sum(axis=1,keepdims=True),bary),axis=1)
 assert bary.min()>-2e-12 and bary.max()<1+2e-12
 return refined,parents,edges

def selected_edges(mesh,selected,method):
 selected=np.asarray(selected,dtype=int)
 if method not in ('longest','all') or not len(selected) or len(selected)>2 or len(np.unique(selected))!=len(selected) or selected.min()<0 or selected.max()>=mesh.nelements:raise ValueError('invalid paired macro marks/method')
 edges=set()
 for i in selected:
  choices=[tuple(sorted(e)) for e in combinations(mesh.t[:,i],2)]
  choices.sort(key=lambda e:(-float(np.sum((mesh.p[:,e[0]]-mesh.p[:,e[1]])**2)),e))
  edges.update(choices[:1] if method=='longest' else choices)
 return edges

def keys(points,cells):return {tuple(sorted(map(tuple,np.round(points[:,t].T,11)))) for t in cells.T}

def refine(octant,selected,method,lo,hi,axes,lengths,old_full):
 refined,parents,edges=split_edges(octant,selected_edges(octant,selected,method))
 removed=np.flatnonzero(np.bincount(parents,minlength=octant.nelements)>1);added=np.flatnonzero(np.isin(parents,removed))
 assert len(removed)>0
 old_local,new_local=alfeld_split(octant),alfeld_split(refined)
 old_idx=np.concatenate([removed+k*octant.nelements for k in range(4)]);new_idx=np.concatenate([added+k*refined.nelements for k in range(4)])
 def measures(mesh,ids):
  shape=shape_inverse(mesh)[ids];volume=np.abs(mesh.mapping().detA[ids]);condition=np.linalg.cond(mesh.mapping().A[:,:,ids].transpose(2,0,1))
  return dict(worst_shape=float(shape.max()),weighted_mean_shape=float(np.average(shape,weights=volume)),max_condition=float(condition.max()),volume_m3=float(volume.sum()/6))
 old,new=measures(old_local,old_idx),measures(new_local,new_idx);np.testing.assert_allclose(old['volume_m3'],new['volume_m3'],rtol=2e-12,atol=1e-14)
 macro=mirror(refined,lengths);mesh=alfeld_split(macro)
 def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
 mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],lengths[0]),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
 mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1);assert np.all(np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)|body(mid))
 assert abs(np.abs(mesh.mapping().detA).sum()/6-31)<1e-9
 areas={}
 for name,facets in mesh.boundaries.items():
  v=mesh.p[:,mesh.facets[:,facets]];areas[name]=float(np.linalg.norm(np.cross((v[:,1]-v[:,0]).T,(v[:,2]-v[:,0]).T),axis=1).sum()/2)
  assert abs(areas[name]-dict(body=6.,inlet=4.,outlet=4.,walls=64.)[name])<1e-9
 original_keys=keys(mesh.p,mesh.t)
 symmetry=True
 for axis in range(3):
  p=mesh.p.copy();p[axis]=lengths[axis]-p[axis];symmetry &= keys(p,mesh.t)==original_keys
 symmetry &= keys(mesh.p[[0,2,1]],mesh.t)==original_keys
 old_global=measures(old_full,np.arange(old_full.nelements));new_global=measures(mesh,np.arange(mesh.nelements))
 metadata=dict(selected=sorted(map(int,selected)),method=method,split_original_edges=[list(e) for e in edges],original_tetrahedra=old_full.nelements,refined_tetrahedra=mesh.nelements,affected_original_macros=len(removed),affected_refined_macros=len(added),affected_original=old,affected_refined=new,global_original=old_global,global_refined=new_global,boundary_areas_m2=areas,all_reflections_and_yz_exchange_preserved=bool(symmetry),macro_parent_partition_verified=True,original_alfeld_tet_partition_claimed=False,physical_cube_and_domain_preserved=True,surface_triangulation_may_refine=True)
 reasons=[]
 if mesh.nelements>50000:reasons.append('mesh cap')
 if not symmetry:reasons.append('reflection/y-z symmetry')
 for key in ('worst_shape','max_condition'):
  if new_global[key]>old_global[key]*(1+1e-8):reasons.append('global '+key+' worsened')
 for key in ('worst_shape','weighted_mean_shape','max_condition'):
  if new[key]>old[key]*(1+1e-8):reasons.append('affected '+key+' worsened')
 if new['weighted_mean_shape']>old['weighted_mean_shape']*.99:reasons.append('affected weighted mean improvement below1%')
 if reasons:raise LocalGeometryRejected(metadata,reasons)
 return (mesh,lo,hi,axes,macro.nelements),metadata
