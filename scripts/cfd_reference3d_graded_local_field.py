"""One archived graded cube; original equations with explicit larger resource contract."""
import argparse,hashlib,json
from pathlib import Path
from unittest.mock import patch
import numpy as np
import cfd_reference3d_accuracy_graded_guarded_probe as probe
from cfd_reference3d_graded_local_survey import build
from cfd_reference3d_accuracy_graded_budget import RSS_CAP,WALL_CAP,PhaseResourceStopped
GEOMETRY_SHA='b3a42a429eb6cdcf2d0a94b1bf8150786757f0b6713bfea4890a1f297b9cc9af'
def run(geometry,library,snapshot,length):
    if length not in (4.,8.) or hashlib.sha256(geometry.read_bytes()).hexdigest()!=GEOMETRY_SHA:raise ValueError('undeclared geometry')
    bundle,_,_=build(length,.04);m,lo,hi,axes,n=bundle;prefix=f'L{int(length)}_r040_'
    with np.load(geometry,allow_pickle=False) as z:
        for k,a in [('vertices_m',m.p),('tetrahedra',m.t),('lo',lo),('hi',hi)]+[(f'axis{i}',a) for i,a in enumerate(axes)]:np.testing.assert_array_equal(z[prefix+k],a)
        assert int(z[prefix+'macro_tetrahedra'])==n
    print(json.dumps(dict(phase='graded_geometry_verified',tetrahedra=m.nelements,length=length,geometry_sha256=GEOMETRY_SHA,rss_cap_bytes=RSS_CAP,wall_cap_s=WALL_CAP)),flush=True)
    with patch.object(probe,'domain_mesh',return_value=bundle):
        row=probe.run(length=length,count=8,split=True,outer_layers=2,target=1e-10,retained_target=1e-11,maxiter=3000,snapshot=snapshot,chunk_size=512,factor_library=library,domain_mesh_mode='held_l4',restart=6,coarse_pressure='quadratic')
    row['accuracy_contract']=dict(rss_cap_bytes=RSS_CAP,wall_cap_s=WALL_CAP,mesh_cap=120000,coefficient_cap=120000000,performance_threshold_applied=False,minimum_factor_savings_applied=False)
    row['geometry_control']=dict(family='graded_genuine_local_edge_refinement',local_edge_radius_m=.04,geometry_sha256=GEOMETRY_SHA,body_triangles_preserved=False,physical_cube_planes_and_areas_preserved=True,first_side_normal_spacing_m=.03125,original_tetrahedron_partition_claimed=False,uniform_refinement_claimed=False)
    return row
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--geometry',type=Path,required=True);ap.add_argument('--length',type=float,choices=(4.,8.),required=True);ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.geometry,a.factor_library,a.snapshot,a.length)
    except PhaseResourceStopped as e:row=dict(numerically_accepted=False,physical_accuracy_certified=False,numerical_failure_reasons=['resources'],resource_phase_rejected=e.record)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row.get(k) for k in ('numerically_accepted','iterations','final_residual','resource_phase_rejected')}),flush=True);raise SystemExit(0 if row.get('numerically_accepted') else 2)
