#!/usr/bin/env python3
"""Non-nested same-surface tensor control with unchanged complete mixed reference solver."""
import argparse,json
from pathlib import Path
from unittest.mock import patch
import cfd_reference3d_cycle_recovery_stage_probe as probe
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
from cfd_reference3d_domain_budget import PhaseResourceStopped

def run(count=6,factor_library=None,snapshot=None,chunk_size=512,restart=6,collect_cycles=False):
    assert count==6
    bundle,geometry=second_normal_tensor_mesh(8.)
    with patch.object(probe,'domain_mesh',return_value=bundle):
        row=probe.run(length=8.,count=count,split=True,outer_layers=2,target=1e-10,maxiter=3000,snapshot=snapshot,chunk_size=chunk_size,factor_library=factor_library,domain_mesh_mode='held_l4',restart=restart,collect_cycles=collect_cycles)
    row['geometry_control']=geometry
    return row

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--collect-cycles',action='store_true');ap.add_argument('--count',type=int,choices=(6,),default=6)
    ap.add_argument('--restart',type=int,choices=(6,),default=6)
    ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--chunk-size',type=int,choices=(512,),default=512)
    ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.count,a.factor_library,a.snapshot,a.chunk_size,a.restart,a.collect_cycles)
    except PhaseResourceStopped as error:row=dict(numerically_accepted=False,diagnostic_accepted=False,physical_accuracy_certified=False,numerical_failure_reasons=['resources'],resource_phase_rejected=error.record,iterations=0,final_residual=None)
    a.output.write_text(json.dumps(row,indent=2)+'\n')
    print(json.dumps({k:row.get(k) for k in ('diagnostic_accepted','numerically_accepted','admission','iterations','final_residual','resource_phase_rejected')}),flush=True)
    raise SystemExit(0 if row.get('diagnostic_accepted') else 2)
