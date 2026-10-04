#!/usr/bin/env python3
"""Bounded geometry-only survey guided by the accepted original signed stress scores."""
import argparse,json,time,resource,hashlib
from pathlib import Path
import numpy as np
from skfem import MeshTet
from scipy.spatial import cKDTree
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_adaptive_mesh import mirror
from cfd_reference3d_mesh import edge_distance
from cfd_reference3d_edge_star_mesh import refine,selected_edges
from cfd_reference3d_corner_local_mesh import LocalGeometryRejected
from cfd_reference3d_domain_budget import enforce_phase,PhaseResourceStopped

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(observer_receipt,geometry_path):
 begin=time.monotonic();r=json.loads(observer_receipt.read_text());assert r['returncode']==0 and r['stop_reason'] is None
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 d=Path(r['command'][r['command'].index('--output')+1]);diagnostic=json.loads(d.read_text());assert diagnostic['diagnostic_accepted']
 inp=Path(diagnostic['input_receipt']);assert sha(inp)==diagnostic['input_receipt_sha256'];fr=json.loads(inp.read_text())
 for q,h in fr['artifact_sha256'].items():assert sha(Path(q))==h
 assert fr['returncode']==0 and fr['stop_reason'] is None
 result=Path(fr['command'][fr['command'].index('--output')+1]);original=json.loads(result.read_text());assert original['numerically_accepted'] and original['final_residual']['true_residual']<1e-10 and original['volume_divergence_max_s_inv']<1e-8 and original['flux_error']<1e-8 and original['physical_energy_imbalance']<.03
 snapshot=Path(fr['command'][fr['command'].index('--snapshot')+1]);axes=[np.array(a) for a in original['axis_nodes_m']];lengths=np.array([8.,2.,2.]);assert original['length']==8. and original['tetrahedra']==33216
 with np.load(snapshot,allow_pickle=False) as z:saved=MeshTet(z['vertices_m'],z['tetrahedra']);lo=z['lo'];hi=z['hi'];assert bool(z['numerically_accepted'])
 octant=MeshTet.init_tensor(*[a[a<=lengths[i]/2+1e-12] for i,a in enumerate(axes)]);c=octant.p[:,octant.t].mean(axis=1);octant=octant.remove_elements(np.flatnonzero(np.all((c>lo[:,None])&(c<hi[:,None]),axis=0)))
 rebuilt=alfeld_split(mirror(octant,lengths));np.testing.assert_array_equal(rebuilt.p,saved.p);np.testing.assert_array_equal(rebuilt.t,saved.t)
 signed=Path(diagnostic['signed_attribution_path']);assert sha(signed)==diagnostic['signed_attribution_sha256']
 with np.load(signed,allow_pickle=False) as z:score=z['cell_score_n']
 assert score.shape==(saved.nelements,) and np.all(np.isfinite(score)) and np.all(score>=0)
 macro_scores=score.reshape(4,-1).sum(axis=0);macro_centers=saved.p[:,np.unique(saved.t.max(axis=0))]
 centers=octant.p[:,octant.t].mean(axis=1).T;tree=cKDTree(centers);folded=np.minimum(macro_centers,lengths[:,None]-macro_centers).T;distance,ids=tree.query(folded);assert distance.max()<1e-10;np.testing.assert_array_equal(np.bincount(ids),np.full(octant.nelements,8))
 orbit=np.bincount(ids,weights=macro_scores);distance,swap=tree.query(centers[:,[0,2,1]]);assert distance.max()<1e-10
 near=np.flatnonzero(edge_distance(centers.T,lo,hi)<=.15);pairs={tuple(sorted((int(i),int(swap[i])))) for i in near};pairs=sorted(pairs,key=lambda pair:(-float(orbit[list(pair)].sum()),pair))
 marks=[];seen=set()
 for pair in pairs:
  edges=tuple(sorted(selected_edges(octant,pair,'longest')))
  if edges in seen:continue
  seen.add(edges);marks.append(pair)
  if len(marks)==8:break
 assert len(marks)==8
 rows=[];winner=None;winning_key=None
 for rank,selected in enumerate(marks):
  for method in ('longest','all'):
   phase=f'rank{rank}-{method}';row=dict(rank=rank,selected=list(selected),method=method,score_n=float(orbit[list(selected)].sum()),centers_m=centers[list(selected)].tolist())
   try:
    bundle,metadata=refine(octant,selected,method,lo,hi,axes,lengths,saved);row.update(geometry_accepted=True,metadata=metadata,reasons=[])
    key=(rank,bundle[0].nelements,method)
    if winner is None or key<winning_key:winner=(bundle,metadata,row.copy());winning_key=key
   except LocalGeometryRejected as error:row.update(geometry_accepted=False,metadata=error.metadata,reasons=error.reasons)
   rows.append(row);print(json.dumps(dict(phase=phase,**row)),flush=True);enforce_phase(phase,resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-begin)
 selected_case=None
 if winner is not None:
  bundle,metadata,selected_case=winner;mesh,blo,bhi,baxes,macros=bundle
  with geometry_path.open('xb') as handle:np.savez_compressed(handle,vertices_m=mesh.p,tetrahedra=mesh.t,lo=blo,hi=bhi,axis0=baxes[0],axis1=baxes[1],axis2=baxes[2],macro_tetrahedra=macros)
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in fr['artifact_sha256'].items():assert sha(Path(q))==h
 return dict(diagnostic_accepted=True,numerically_accepted=False,numeric_factor_attempted=False,numerical_field_published=False,physical_accuracy_certified=False,input_observer_receipt=str(observer_receipt),input_observer_receipt_sha256=sha(observer_receipt),input_receipt=str(inp),input_receipt_sha256=sha(inp),input_snapshot_sha256=sha(snapshot),input_signed_attribution_sha256=sha(signed),original_saved_mesh_rebuilt_bitwise=True,candidates=rows,selected_case=selected_case,geometry_path=str(geometry_path) if winner is not None else None,geometry_sha256=sha(geometry_path) if winner is not None else None,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,wall_s=time.monotonic()-begin,scope='geometry-only, parent-partition and quality survey; no numerical field or force certification')
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--input-observer-receipt',type=Path,required=True);ap.add_argument('--geometry',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.geometry.exists()
 try:row=run(a.input_observer_receipt,a.geometry)
 except PhaseResourceStopped as error:row=dict(diagnostic_accepted=False,numerically_accepted=False,numeric_factor_attempted=False,numerical_field_published=False,physical_accuracy_certified=False,resource_phase_rejected=error.record)
 a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(dict(diagnostic_accepted=row['diagnostic_accepted'],selected_case=row.get('selected_case'),wall_s=row.get('wall_s'))),flush=True);raise SystemExit(0 if row['diagnostic_accepted'] else 2)
