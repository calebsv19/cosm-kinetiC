#!/usr/bin/env python3
"""Once-only bounded transition-quality survey; no PDE factor allocation."""
import argparse,hashlib,json,resource,shutil,time
from pathlib import Path
from cfd_reference3d_force_transition import force_transition_mesh,LocalGeometryRejected,FRACTIONS
from cfd_reference3d_domain_budget import enforce_phase
ROOT=Path(__file__).resolve().parents[1]
SOURCES=('cfd_reference3d_force_transition_survey.py','cfd_reference3d_force_transition.py','cfd_reference3d_force_local_mesh.py','cfd_reference3d_corner_local_mesh.py','cfd_reference3d_adaptive_mesh.py','cfd_reference3d_p3.py','cfd_reference3d_preconditioner.py','cfd_reference3d_mesh.py','cfd_reference3d_domain_budget.py')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(diagnostic,output):
    assert not output.exists();begin=time.monotonic();rows=[]
    hashes={name:sha(ROOT/'scripts'/name) for name in SOURCES};digest=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    frozen=output.parent/'geometry-source'/digest;frozen.mkdir(parents=True,exist_ok=True)
    for name,h in hashes.items():
        p=frozen/name
        if not p.exists():shutil.copy2(ROOT/'scripts'/name,p)
        assert sha(p)==h
    for rank in range(8):
        try:
            mesh,metadata=force_transition_mesh(diagnostic,rank);del mesh
            row=dict(geometry_admitted=True,metadata=metadata,reasons=[])
        except LocalGeometryRejected as error:row=dict(geometry_admitted=False,metadata=error.metadata,reasons=error.reasons)
        rows.append(row)
        enforce_phase('transition_survey',resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-begin)
        print(json.dumps(dict(rank=rank,geometry_admitted=row['geometry_admitted'],original_worst_shape=row['metadata']['original_affected_worst_shape'],refined_worst_shape=row['metadata']['refined_affected_worst_shape'],reasons=row['reasons'])),flush=True)
    output.write_text(json.dumps(dict(schema='physics_sim_c3d_force_transition_survey_v1',candidates=rows,source_sha256=hashes,frozen_source=str(frozen),diagnostic_path=str(diagnostic),diagnostic_sha256=sha(diagnostic),fractions=list(FRACTIONS),pass_cap=4,wall_s=time.monotonic()-begin,owned_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,numeric_factor_attempted=False,physical_accuracy_certified=False),indent=2)+'\n')
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--diagnostic',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();run(a.diagnostic,a.output)
