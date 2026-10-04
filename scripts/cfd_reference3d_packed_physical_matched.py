"""Earned exact matched cube control; original physical/resource gates retained."""
import argparse,json
from pathlib import Path
from unittest.mock import patch
import cfd_reference3d_packed_physical_probe as probe
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
from cfd_reference3d_domain_budget import PhaseResourceStopped

def run(count=6,factor_library=None,coarse_library=None,snapshot=None,chunk_size=512,restart=30):
    if count!=6 or chunk_size!=512 or restart!=30:raise ValueError('unsupported matched recipe')
    bundle,geometry=second_normal_tensor_mesh(4.)
    if bundle[0].nelements!=28416:raise ValueError('matched geometry identity changed')
    with patch.object(probe,'domain_mesh',return_value=bundle):
        row=probe.run(length=4.,count=count,split=True,outer_layers=1,target=1e-10,retained_target=1e-11,maxiter=3000,snapshot=snapshot,chunk_size=chunk_size,factor_library=factor_library,coarse_library=coarse_library,domain_mesh_mode='held_l4',restart=restart,coarse_pressure='quadratic',storage_mode='auto')
    row['geometry_control']=geometry
    return row
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--count',type=int,choices=(6,),default=6);ap.add_argument('--restart',type=int,choices=(30,),default=30);ap.add_argument('--chunk-size',type=int,choices=(512,),default=512)
    ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--coarse-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.count,a.factor_library,a.coarse_library,a.snapshot,a.chunk_size,a.restart)
    except PhaseResourceStopped as e:row=dict(numerically_accepted=False,diagnostic_accepted=False,physical_accuracy_certified=False,numerical_failure_reasons=['resources'],resource_phase_rejected=e.record,iterations=0,final_residual=None)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row.get(k) for k in ('numerically_accepted','iterations','final_residual','resource_phase_rejected')}),flush=True);raise SystemExit(0 if row.get('numerically_accepted') else 2)
