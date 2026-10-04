"""Genuine conforming edge refinement, retaining original graded tensor background."""
import json,hashlib,time,resource
from pathlib import Path
import numpy as np
from cfd_reference3d_accuracy_graded_mesh import build as parent_build,translated_inner_keys
from cfd_reference3d_relative_octant import refined_octant
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_edge_redistribution_mesh import quality
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-graded-local-relative'
RADII=(.025,.04)
def build(length,radius):
    if length not in (4.,8.) or radius not in RADII:raise ValueError('undeclared local geometry')
    old,_=parent_build(length);_,lo,hi,axes,_=old
    macro,counts=refined_octant(axes,np.array([length,2.,2.]),lo,hi,1,radii=[radius]);m=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    m=m.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    return (m,lo,hi,axes,macro.nelements),old,counts

def run():
    D.mkdir(exist_ok=False);out=D/'geometry-survey.json';archive=D/'geometry.npz';rows=[];arrays={};inners={};start=time.monotonic()
    for L in (4.,8.):
        old,_=parent_build(L);baseline=quality(old[0],old[1],old[2],L)
        for radius in RADII:
            bundle,_,counts=build(L,radius);m,lo,hi,axes,n=bundle;q=quality(m,lo,hi,L);reasons=[]
            physical=bool(q['physical_boundary_planes_preserved'] and abs(q['global_quality']['volume_m3']-(4*L-1))<1e-9 and all(abs(q['boundary_areas_m2'][k]-v)<1e-9 for k,v in dict(body=6.,inlet=4.,outlet=4.,walls=8*L).items()))
            if not physical:reasons.append('physical boundary/area/volume')
            if not q['all_reflections_and_yz_exchange_preserved']:reasons.append('actual cell symmetry/YZ exchange')
            if m.nelements>120000:reasons.append('mesh cap')
            for k in ('worst_shape','max_condition'):
                if q['global_quality'][k]>baseline['global_quality'][k]*(1+1e-8):reasons.append('global '+k+' worsened')
            kind=f'r{round(radius*1000):03d}';prefix=f'L{int(L)}_{kind}_';arrays.update({prefix+'vertices_m':m.p,prefix+'tetrahedra':m.t,prefix+'lo':lo,prefix+'hi':hi,prefix+'macro_tetrahedra':n});arrays.update({prefix+f'axis{i}':a for i,a in enumerate(axes)});inners[L,kind]=translated_inner_keys(bundle)
            row=dict(length=L,radius_m=radius,kind=kind,quality=q,parent_quality=baseline,counts=counts,physical_geometry_passed=physical,geometry_accepted=not reasons,reasons=reasons,uniform_refinement_claimed=False,numerical_factor_attempted=False,physical_accuracy_certified=False);rows.append(row)
            print(json.dumps(dict(phase='local_geometry',length=L,radius_m=radius,tetrahedra=m.nelements,accepted=not reasons,reasons=reasons)),flush=True)
            assert resource.getrusage(resource.RUSAGE_SELF).ru_maxrss<2048*2**20 and time.monotonic()-start<600
    paired={kind:inners[4.,kind]==inners[8.,kind] for kind in ('r025','r040')}
    with archive.open('xb') as f:np.savez_compressed(f,**arrays)
    result=dict(schema='physics_sim_c3d_graded_local_geometry_v1',rows=rows,translated_inner_cells_match=paired,archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),rss_cap_bytes=2048*2**20,wall_cap_s=600,wall_s=time.monotonic()-start,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,numerical_factor_attempted=False,flow_field_published=False)
    assert result['peak_rss_bytes']<2048*2**20 and result['wall_s']<600
    out.write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':run()
