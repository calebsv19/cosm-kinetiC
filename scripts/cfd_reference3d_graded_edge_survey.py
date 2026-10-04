"""Prospective graded edge/normal geometries; no solve or force-error certification."""
import json,hashlib,time,resource
from pathlib import Path
import numpy as np
from cfd_reference3d_accuracy_graded_mesh import build as parent_build,translated_inner_keys
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_edge_redistribution_mesh import quality
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-graded-stress'
CONTROLS={'edge050':(.05,.03125),'edge040':(.04,.03125),'normal025':(None,.025),'coupled025':(.05,.025)}
def build(length,kind):
    if length not in (4.,8.) or kind not in CONTROLS:raise ValueError('undeclared geometry')
    old,_=parent_build(length);_,lo,hi,axes,_=old;spacing,normal=CONTROLS[kind];changed=[]
    for axis,a in enumerate(axes):
        b=a.copy()
        if spacing is not None:
            for oldplane,newplane in ((lo[axis]+.5*(1-np.cos(np.pi/6)),lo[axis]+spacing),(hi[axis]-.5*(1-np.cos(np.pi/6)),hi[axis]-spacing)):
                mask=np.isclose(b,oldplane,rtol=0,atol=1e-12);assert mask.sum()==1;b[mask]=newplane
        if axis in (1,2):
            for oldplane,newplane in ((lo[axis]-.03125,lo[axis]-normal),(hi[axis]+.03125,hi[axis]+normal)):
                mask=np.isclose(b,oldplane,rtol=0,atol=1e-12);assert mask.sum()==1;b[mask]=newplane
        assert np.all(np.diff(b)>0);changed.append(b)
    macro,_=refined_octant(changed,np.array([length,2.,2.]),lo,hi,0);m=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    m=m.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    assert m.nelements==old[0].nelements and len(m.boundaries['body'])==len(old[0].boundaries['body'])
    return (m,lo,hi,changed,macro.nelements),old

def run():
    out=D/'edge-geometry-survey.json';archive=D/'edge-geometry.npz';assert not out.exists() and not archive.exists()
    started=time.monotonic();records=[];arrays={};inner={}
    for length in (4.,8.):
        old,_=parent_build(length);baseline=quality(old[0],old[1],old[2],length)
        for kind in CONTROLS:
            bundle,_=build(length,kind);m,lo,hi,axes,n=bundle;q=quality(m,lo,hi,length)
            physical=bool(q['all_reflections_and_yz_exchange_preserved'] and q['physical_boundary_planes_preserved'] and abs(q['global_quality']['volume_m3']-(4*length-1))<1e-9 and all(abs(q['boundary_areas_m2'][k]-v)<1e-9 for k,v in dict(body=6.,inlet=4.,outlet=4.,walls=8*length).items()))
            assert physical and q['tetrahedra']<=120000
            prefix=f'L{int(length)}_{kind}_';arrays.update({prefix+'vertices_m':m.p,prefix+'tetrahedra':m.t,prefix+'lo':lo,prefix+'hi':hi,prefix+'macro_tetrahedra':n});arrays.update({prefix+f'axis{i}':a for i,a in enumerate(axes)})
            inner[(length,kind)]=translated_inner_keys(bundle)
            # Screen against the previous successful graded geometry; report
            # every quality change, without mistaking condition for force error.
            reasons=[]
            for key in ('worst_shape','max_condition'):
                if q['global_quality'][key]>baseline['global_quality'][key]*(1+1e-8):reasons.append('global '+key+' worsened')
            row=dict(length=length,kind=kind,quality=q,parent_quality=baseline,physical_geometry_passed=physical,prospective_quality_passed=not reasons,reasons=reasons,uniform_refinement_claimed=False,numerical_factor_attempted=False,physical_accuracy_certified=False)
            records.append(row);print(json.dumps(dict(phase='geometry',length=length,kind=kind,tetrahedra=m.nelements,global_quality=q['global_quality'],accepted=not reasons)),flush=True)
            assert resource.getrusage(resource.RUSAGE_SELF).ru_maxrss<2048*2**20 and time.monotonic()-started<600
    paired={kind:inner[(4.,kind)]==inner[(8.,kind)] for kind in CONTROLS};assert all(paired.values())
    with archive.open('xb') as f:np.savez_compressed(f,**arrays)
    assert resource.getrusage(resource.RUSAGE_SELF).ru_maxrss<2048*2**20 and time.monotonic()-started<600
    result=dict(schema='physics_sim_c3d_graded_edge_geometry_survey_v1',records=records,translated_inner_cells_match=paired,archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),rss_cap_bytes=2048*2**20,wall_cap_s=600,wall_s=time.monotonic()-started,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,numerical_factor_attempted=False,flow_field_published=False)
    out.write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':run()
