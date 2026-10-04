#!/usr/bin/env python3
"""Bounded once-only prospective geometry survey; no matrix/factor/field solve."""
import argparse,hashlib,json,resource,time
from pathlib import Path
from cfd_reference3d_force_local_mesh import force_local_mesh,LocalGeometryRejected
from cfd_reference3d_domain_budget import enforce_phase
ROOT=Path(__file__).resolve().parents[1]
SOURCES=('cfd_reference3d_force_local_survey.py','cfd_reference3d_force_local_mesh.py','cfd_reference3d_corner_local_mesh.py','cfd_reference3d_adaptive_mesh.py','cfd_reference3d_p3.py','cfd_reference3d_preconditioner.py','cfd_reference3d_mesh.py','cfd_reference3d_domain_budget.py')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(diagnostic,output):
    assert not output.exists();begin=time.monotonic();rows=[]
    for rank in range(8):
        for method in ('bisection','edge_star'):
            try:
                mesh,metadata=force_local_mesh(diagnostic,rank,method);del mesh
                rows.append(dict(geometry_admitted=True,metadata=metadata,reasons=[]))
            except LocalGeometryRejected as error:rows.append(dict(geometry_admitted=False,metadata=error.metadata,reasons=error.reasons))
            enforce_phase('geometry_survey',resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-begin)
            print(json.dumps(dict(rank=rank,method=method,geometry_admitted=rows[-1]['geometry_admitted'],reasons=rows[-1]['reasons'])),flush=True)
    output.write_text(json.dumps(dict(schema='physics_sim_c3d_force_local_geometry_survey_v1',candidates=rows,source_sha256={name:sha(ROOT/'scripts'/name) for name in SOURCES},diagnostic_path=str(diagnostic),diagnostic_sha256=sha(diagnostic),wall_s=time.monotonic()-begin,owned_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,numeric_factor_attempted=False,physical_accuracy_certified=False),indent=2)+'\n')
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--diagnostic',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();run(a.diagnostic,a.output)
