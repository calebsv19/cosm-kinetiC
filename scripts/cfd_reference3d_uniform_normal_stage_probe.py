#!/usr/bin/env python3
"""Uniform-normal geometry control with unchanged complete mixed reference solver."""
import argparse,json
from pathlib import Path
from unittest.mock import patch
import cfd_reference3d_mixed_precision_stage_probe as probe
from cfd_reference3d_uniform_normal_mesh import uniform_normal_mesh
from cfd_reference3d_domain_budget import PhaseResourceStopped

def run(count=6,factor_library=None,snapshot=None,chunk_size=512):
    bundle,geometry=uniform_normal_mesh(4.,count)
    with patch.object(probe,'domain_mesh',return_value=bundle):
        row=probe.run(length=4.,count=count,split=False,outer_layers=2,target=1e-10,maxiter=3000,snapshot=snapshot,chunk_size=chunk_size,factor_library=factor_library,domain_mesh_mode='original')
    row['geometry_control']=geometry
    return row

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--count',type=int,choices=(6,8),default=6)
    ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--chunk-size',type=int,choices=(512,),default=512)
    ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.count,a.factor_library,a.snapshot,a.chunk_size)
    except PhaseResourceStopped as error:row=dict(numerically_accepted=False,diagnostic_accepted=False,physical_accuracy_certified=False,numerical_failure_reasons=['resources'],resource_phase_rejected=error.record,iterations=0,final_residual=None)
    a.output.write_text(json.dumps(row,indent=2)+'\n')
    print(json.dumps({k:row.get(k) for k in ('diagnostic_accepted','numerically_accepted','admission','iterations','final_residual','resource_phase_rejected')}),flush=True)
    raise SystemExit(0 if row.get('diagnostic_accepted') else 2)
