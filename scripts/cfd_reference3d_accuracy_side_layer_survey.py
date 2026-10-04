"""Two fixed side-normal geometry controls; no equations, factorization or fields."""
import argparse,json,resource,time,hashlib
from pathlib import Path
import numpy as np
from cfd_reference3d_floor_balanced8_mesh import build as parent_build,SPACINGS
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_second_normal_mesh import facet_keys
from cfd_reference3d_edge_redistribution_mesh import quality
from cfd_reference3d_accuracy_budget import enforce_phase

def build(length,mode):
    if length not in (4.,8.) or mode not in ('redistribute','bisect'):raise ValueError('undeclared geometry control')
    old,lo,hi,axes,n=parent_build(length,SPACINGS[0]);changed=[a.copy() for a in axes]
    for i in (1,2):
        closest=axes[i][axes[i]<lo[i]-1e-12][-1];assert abs(lo[i]-closest-.0625)<1e-12
        plane=lo[i]-.03125;mirror=hi[i]+.03125
        if mode=='bisect':changed[i]=np.sort(np.r_[axes[i],plane,mirror])
        else:
            changed[i][np.isclose(axes[i],closest)]=plane
            changed[i][np.isclose(axes[i],hi[i]+.0625)]=mirror
        assert np.all(np.diff(changed[i])>0)
    macro,_=refined_octant(changed,np.array([length,2.,2.]),lo,hi,0);mesh=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    assert facet_keys(mesh,mesh.boundaries['body'])==facet_keys(old,old.boundaries['body'])
    return (mesh,lo,hi,changed,macro.nelements),(old,lo,hi,axes,n)

def run(output,geometry):
    start=time.monotonic();rows=[];arrays={}
    for length in (4.,8.):
        original=None
        for mode in ('redistribute','bisect'):
            bundle,parent=build(length,mode);m,lo,hi,axes,n=bundle
            if original is None:original=quality(parent[0],lo,hi,length)
            q=quality(m,lo,hi,length)
            assert q['tetrahedra']<=75000 and q['all_reflections_and_yz_exchange_preserved'] and q['physical_boundary_planes_preserved']
            assert abs(q['global_quality']['volume_m3']-(4*length-1))<1e-9
            for k,v in dict(body=6.,inlet=4.,outlet=4.,walls=8*length).items():assert abs(q['boundary_areas_m2'][k]-v)<1e-9
            prefix=f'L{int(length)}_{mode}_';arrays.update({prefix+'vertices_m':m.p,prefix+'tetrahedra':m.t,prefix+'lo':lo,prefix+'hi':hi,prefix+'macro_tetrahedra':n})
            arrays.update({prefix+f'axis{i}':a for i,a in enumerate(axes)})
            row=dict(length=length,mode=mode,first_side_normal_spacing_m=.03125,previous_first_side_normal_spacing_m=.0625,quality=q,original_quality=original,body_surface_triangles_preserved=True,original_tetrahedron_partition_claimed=False,geometry_invariants_passed=True,within_existing_numerical_mesh_cap=m.nelements<=50000,numerical_factor_admission_checked=False,physical_accuracy_certified=False)
            rows.append(row);enforce_phase('side_layer_geometry',resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-start)
            print(json.dumps(dict(phase='side_layer_geometry',length=length,mode=mode,tetrahedra=m.nelements,max_condition=q['global_quality']['max_condition'],worst_shape=q['global_quality']['worst_shape'])),flush=True)
    assert not geometry.exists() and not output.exists()
    with geometry.open('xb') as f:np.savez_compressed(f,**arrays)
    enforce_phase('geometry_archive',resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-start)
    result=dict(schema='physics_sim_c3d_accuracy_side_layer_survey_v1',rows=rows,geometry_archive=str(geometry.resolve()),geometry_sha256=hashlib.sha256(geometry.read_bytes()).hexdigest(),geometry_survey_cap_tetrahedra=75000,original_numerical_mesh_cap_tetrahedra=50000,rss_cap_bytes=3072*2**20,wall_cap_s=600,wall_s=time.monotonic()-start,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,numerical_factor_attempted=False,flow_field_published=False,physical_accuracy_certified=False)
    output.write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);ap.add_argument('--geometry',type=Path,required=True);a=ap.parse_args();run(a.output,a.geometry)
