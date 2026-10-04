"""One archived graded cube; original equations with explicit larger resource contract."""
import argparse,hashlib,json
from pathlib import Path
from unittest.mock import patch
import numpy as np
import cfd_reference3d_accuracy_graded_guarded_probe as probe
from skfem import MeshTet
from cfd_reference3d_accuracy_graded_budget import RSS_CAP,WALL_CAP,PhaseResourceStopped
GEOMETRY_SHA='391ff6f69c3cb3e3009b9c68f94a835802fbbd8528ee52987b48c054a8eaa9ca'
SELECTION_SHA='617778d93d448684dcef8c46cdf3e92f3a987ccc9b7c00a7359f15247ded5a6e'
def build(geometry,length):
    selection_root=geometry.parent.parent/'c3d-directional-fine-field'
    selection=selection_root/'selection-contract.json'
    assert hashlib.sha256(selection.read_bytes()).hexdigest()==SELECTION_SHA
    admission=json.loads((geometry.parent/'assessment.json').read_text())
    assert admission['geometry_sha256']==GEOMETRY_SHA and 'matched_edge031' in admission['accepted_paired_profiles']
    if length==8.:
        previous=json.loads((selection_root/'L4-physical-assessment.json').read_text())
        assert previous['L8_trial_permitted'] and previous['selection_contract_sha256']==SELECTION_SHA
        assert hashlib.sha256(Path(previous['receipt']).read_bytes()).hexdigest()==previous['receipt_sha256']
    prefix=f'L{int(length)}_matched_edge031_'
    with np.load(geometry,allow_pickle=False) as z:
        vertices=z[prefix+'vertices_m'].copy();tetrahedra=z[prefix+'tetrahedra'].copy()
        lo=z[prefix+'lo'].copy();hi=z[prefix+'hi'].copy()
        axes=[z[prefix+'axis'+str(i)].copy() for i in range(3)]
        n=int(z[prefix+'macro_tetrahedra'])
    m=MeshTet(vertices,tetrahedra)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    m=m.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    np.testing.assert_array_equal(m.p,vertices);np.testing.assert_array_equal(m.t,tetrahedra)
    assert m.nelements==(81792 if length==4. else 100608) and m.nelements==4*n
    return m,lo,hi,axes,n

def run(geometry,library,snapshot,length):
    if length not in (4.,8.) or hashlib.sha256(geometry.read_bytes()).hexdigest()!=GEOMETRY_SHA:raise ValueError('undeclared geometry')
    bundle=build(geometry,length);m,lo,hi,axes,n=bundle;prefix=f'L{int(length)}_matched_edge031_'
    with np.load(geometry,allow_pickle=False) as z:
        for k,a in [('vertices_m',m.p),('tetrahedra',m.t),('lo',lo),('hi',hi)]+[(f'axis{i}',a) for i,a in enumerate(axes)]:np.testing.assert_array_equal(z[prefix+k],a)
        assert int(z[prefix+'macro_tetrahedra'])==n
    print(json.dumps(dict(phase='graded_geometry_verified',tetrahedra=m.nelements,length=length,geometry_sha256=GEOMETRY_SHA,rss_cap_bytes=RSS_CAP,wall_cap_s=WALL_CAP)),flush=True)
    with patch.object(probe,'domain_mesh',return_value=bundle):
        row=probe.run(length=length,count=8,split=True,outer_layers=2,target=1e-10,retained_target=1e-11,maxiter=3000,snapshot=snapshot,chunk_size=512,factor_library=library,domain_mesh_mode='held_l4',restart=6,coarse_pressure='quadratic')
    row['accuracy_contract']=dict(rss_cap_bytes=RSS_CAP,wall_cap_s=WALL_CAP,mesh_cap=120000,coefficient_cap=120000000,performance_threshold_applied=False,minimum_factor_savings_applied=False)
    row['geometry_control']=dict(family='directional_tensor_redistribution',first_front_normal_spacing_m=.03125,first_surface_edge_spacing_m=.03125,geometry_sha256=GEOMETRY_SHA,body_triangles_preserved=False,physical_cube_planes_and_areas_preserved=True,first_side_normal_spacing_m=.03125,original_tetrahedron_partition_claimed=False,uniform_refinement_claimed=False)
    return row
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--geometry',type=Path,required=True);ap.add_argument('--length',type=float,choices=(4.,8.),required=True);ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.geometry,a.factor_library,a.snapshot,a.length)
    except PhaseResourceStopped as e:row=dict(numerically_accepted=False,physical_accuracy_certified=False,numerical_failure_reasons=['resources'],resource_phase_rejected=e.record)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row.get(k) for k in ('numerically_accepted','iterations','final_residual','resource_phase_rejected')}),flush=True);raise SystemExit(0 if row.get('numerically_accepted') else 2)
