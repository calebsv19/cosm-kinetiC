"""One accuracy-first fine cube; no performance or savings qualification gate."""
import argparse,hashlib,json
from pathlib import Path
from unittest.mock import patch
import numpy as np
import cfd_reference3d_accuracy_probe as probe
from cfd_reference3d_floor_balanced8_mesh import build,SPACINGS
from cfd_reference3d_accuracy_budget import PhaseResourceStopped,RSS_CAP,WALL_CAP
GEOMETRY_SHA='27a4ad58c42d6932f08cbe5feb4aba78457ec9a64e4f7dc669d962f9f7108ea8'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(geometry,factor_library,snapshot,length=4.):
    if length not in (4.,8.) or sha(geometry)!=GEOMETRY_SHA:raise ValueError('unsupported length or changed qualified geometry')
    bundle=build(length,SPACINGS[0]);mesh,lo,hi,axes,n=bundle;prefix='L4_' if length==4. else 'L8_'
    with np.load(geometry,allow_pickle=False) as z:
        for key,value in (('vertices_m',mesh.p),('tetrahedra',mesh.t),('lo',lo),('hi',hi),('axis0',axes[0]),('axis1',axes[1]),('axis2',axes[2])):np.testing.assert_array_equal(z[prefix+key],value)
        assert int(z[prefix+'macro_tetrahedra'])==n
    print(json.dumps(dict(phase='accuracy_geometry_verified',geometry_path=str(geometry),geometry_sha256=GEOMETRY_SHA,tetrahedra=mesh.nelements,rss_cap_bytes=RSS_CAP,wall_cap_s=WALL_CAP,performance_threshold_applied=False)),flush=True)
    with patch.object(probe,'domain_mesh',return_value=bundle):
        row=probe.run(length=length,count=8,split=True,outer_layers=2,target=1e-10,retained_target=1e-11,maxiter=3000,snapshot=snapshot,chunk_size=512,factor_library=factor_library,domain_mesh_mode='held_l4',restart=6,coarse_pressure='quadratic')
    row['accuracy_contract']=dict(rss_cap_bytes=RSS_CAP,wall_cap_s=WALL_CAP,performance_threshold_applied=False,minimum_factor_savings_applied=False,user_priority='physical accuracy first; efficiency later')
    row['geometry_control']=dict(family='floor_balanced_eight_interval',geometry_sha256=GEOMETRY_SHA,body_intervals=8,first_strip_m=SPACINGS[0],original_macro_partition_claimed=False,uniform_refinement_claimed=False)
    return row
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--geometry',type=Path,required=True);ap.add_argument('--length',type=float,choices=(4.,8.),default=4.);ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.geometry,a.factor_library,a.snapshot,a.length)
    except PhaseResourceStopped as e:row=dict(numerically_accepted=False,physical_accuracy_certified=False,numerical_failure_reasons=['resources'],resource_phase_rejected=e.record,iterations=0,final_residual=None)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row.get(k) for k in ('numerically_accepted','iterations','final_residual','resource_phase_rejected')}),flush=True);raise SystemExit(0 if row.get('numerically_accepted') else 2)
