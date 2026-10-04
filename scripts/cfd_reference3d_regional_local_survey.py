"""Screen genuine two-pass edge refinement against regional and global quality."""
import json
from pathlib import Path
import resource
import time
import numpy as np
from cfd_reference3d_accuracy_graded_mesh import build as parent_build
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_edge_redistribution_mesh import quality
from cfd_reference3d_translated_cells import compare_cells,inner_cells
from run_cfd_native_accuracy_regression import save,sha
ROOT=Path(__file__).resolve().parents[1];DEST=ROOT/'build/c3d-regional-local'
PROFILES={'two040':(.04,.04),'two040025':(.04,.025)}
def main():
 DEST.mkdir(exist_ok=False);started=time.monotonic();arrays={};rows=[];source_names=(Path(__file__).relative_to(ROOT).as_posix(),'scripts/cfd_reference3d_accuracy_graded_mesh.py','scripts/cfd_reference3d_mesh.py','scripts/cfd_reference3d_p3.py','scripts/cfd_reference3d_edge_redistribution_mesh.py','scripts/cfd_reference3d_translated_cells.py')
 hashes={q:sha(ROOT/q) for q in source_names}
 save(DEST/'contract.json',dict(source_sha256=hashes,profiles=PROFILES,mesh_cap=120000,rss_cap_bytes=2048*1024**2,wall_cap_s=600,maximum_regional_shape_condition_ratio=1+1e-8,numerical_factor_attempted=False))
 for length in (4.,8.):
  parent,_=parent_build(length);_,lo,hi,axes,_=parent;before=quality(parent[0],lo,hi,length)
  for name,radii in PROFILES.items():
   macro,counts=refined_octant(axes,np.array([length,2.,2.]),lo,hi,2,radii=list(radii));mesh=alfeld_split(macro)
   def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
   mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
   after=quality(mesh,lo,hi,length);reasons=[]
   if mesh.nelements>120000:reasons.append('mesh cap')
   if not(after['physical_boundary_planes_preserved'] and after['all_reflections_and_yz_exchange_preserved']):reasons.append('physical boundaries/symmetry')
   if abs(after['global_quality']['volume_m3']-(4*length-1))>1e-9:reasons.append('fluid volume')
   if any(abs(after['boundary_areas_m2'][k]-v)>1e-9 for k,v in dict(body=6.,inlet=4.,outlet=4.,walls=8*length).items()):reasons.append('boundary area')
   ratios={}
   for region in ('global','edge-0.025','edge-0.05','edge-0.1','body-0.05'):
    a=before['global_quality'] if region=='global' else before['bands'][region];b=after['global_quality'] if region=='global' else after['bands'][region]
    ratios[region]={k:b[k]/a[k] for k in ('worst_shape','max_condition')}
    for k,ratio in ratios[region].items():
     if ratio>1+1e-8:reasons.append(region+' '+k+' worsened')
   prefix=f'L{int(length)}_{name}_';arrays.update({prefix+'vertices_m':mesh.p,prefix+'tetrahedra':mesh.t,prefix+'lo':lo,prefix+'hi':hi})
   row=dict(length=length,profile=name,counts=counts,tetrahedra=mesh.nelements,parent_quality=before,quality=after,quality_ratios=ratios,geometry_screen_passed=not reasons,reasons=reasons,numerical_factor_attempted=False,flow_field_published=False);rows.append(row)
   print(json.dumps({k:row[k] for k in ('length','profile','tetrahedra','geometry_screen_passed','reasons')}),flush=True)
   assert time.monotonic()-started<600 and resource.getrusage(resource.RUSAGE_SELF).ru_maxrss<2048*1024**2
 paired={name:compare_cells(inner_cells(arrays,'L4_'+name+'_'),inner_cells(arrays,'L8_'+name+'_')) for name in PROFILES}
 with (DEST/'geometry.npz').open('xb') as f:np.savez_compressed(f,**arrays)
 for q,h in hashes.items():assert sha(ROOT/q)==h
 save(DEST/'assessment.json',dict(status='completed_regional_geometry_screen',source_sha256=hashes,rows=rows,actual_inner_cell_comparison=paired,geometry_sha256=sha(DEST/'geometry.npz'),wall_s=time.monotonic()-started,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,numerical_factor_attempted=False,flow_field_published=False,physical_accuracy_certified=False))
 print(json.dumps(dict(status='completed_regional_geometry_screen',accepted=sum(r['geometry_screen_passed'] for r in rows),paired=paired)),flush=True)
if __name__=='__main__':main()
