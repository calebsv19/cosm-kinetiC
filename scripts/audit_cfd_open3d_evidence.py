#!/usr/bin/env python3
"""Readback audit: exported open faces/pressure independently reproduce flux and traces."""
import hashlib
import json
import math
from pathlib import Path
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/c3d-open'
sys.path.insert(0,str(ROOT/'scripts'))
from qualify_cfd_open3d import gate

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    numerical=json.loads((OUT/'qualification.json').read_text())
    agent=json.loads((OUT/'agent-evidence/qualification.json').read_text())
    assert numerical['passed'] and agent['passed']
    assert numerical['binary_sha256']==sha(OUT/'open3d_test')
    assert agent['worker_sha256']==sha(ROOT/'build/cfd-optimized/physics_sim_session_worker')
    assert [gate(row) for row in numerical['grids']]==[False,False,True]
    assert all(gate(row) for row in numerical['outlet_extensions']) and gate(numerical['rectangle'],.009)
    artifacts=0;checks=[]
    for record in agent['records']:
        for artifact in record['result']['artifacts']:
            assert sha(artifact['path'])==artifact['sha256'];artifacts+=1
        field=next(a for a in record['result']['artifacts'] if a['path'].endswith('channel_fields.json'))
        data=json.loads(Path(field['path']).read_text())['cartesian_fields']
        assert all(int(x)==x for x in record['grid'])
        nx,ny,nz=map(int,record['grid']);dx,dy,dz=data['spacing_m'];v=data['velocity_faces_m_s'];p=data['pressure_pa'];out=data['outlet_x_velocity_m_s']
        assert len(v)==len(p)==nx*ny*nz and len(out)==ny*nz
        assert all(math.isfinite(x) for row in v for x in row) and all(math.isfinite(x) for x in p+out)
        pin1=pin2=pout1=pout2=qin=qout=maxdiv=0.
        for k in range(nz):
            for j in range(ny):
                base=(k*ny+j)*nx
                pin1+=p[base];pin2+=p[base+1];pout1+=p[base+nx-1];pout2+=p[base+nx-2]
                qin+=v[base][0]*dy*dz;qout+=out[k*ny+j]*dy*dz
                for i in range(nx):
                    q=base+i
                    east=v[q+1][0] if i<nx-1 else out[k*ny+j]
                    north=v[q+nx][1] if j<ny-1 else 0
                    upper=v[q+nx*ny][2] if k<nz-1 else 0
                    div=(east-v[q][0])/dx+(north-v[q][1])/dy+(upper-v[q][2])/dz
                    maxdiv=max(maxdiv,abs(div))
        ptrace_in=(3*pin1-pin2)/(2*ny*nz);ptrace_out=(3*pout1-pout2)/(2*ny*nz)
        assert abs(ptrace_in-record['physics']['inlet_pressure_trace_pa'])<1e-12
        assert abs(ptrace_out-record['physics']['outlet_pressure_trace_pa'])<1e-12
        assert abs(ptrace_in-ptrace_out-record['physics']['pressure_drop_pa'])<1e-12
        assert abs(qout-record['health']['volume_flux_m3_s'])<1e-12
        assert abs(qin-qout)/qin<1e-10 and maxdiv<1e-8
        checks.append({'run_id':record['run_id'],'inlet_trace_pa':ptrace_in,'outlet_trace_pa':ptrace_out,'inlet_flux_m3_s':qin,'outlet_flux_m3_s':qout,'max_divergence_s_inv':maxdiv,'passed':True})
    previous=json.loads((ROOT/'build/c3d/completion-audit.json').read_text())
    preserved={f:sha(ROOT/f)==previous['source_sha256'][f] for f in ('src/app/cfd_cartesian3d.c','src/app/cfd_duct3d.c','src/app/cfd_periodic3d.c','src/app/cfd_sparse_mg.c')}
    assert all(preserved.values())
    logs=['final-unit.log','sanitize.log','2d-regression.log','periodic3d-regression.log','periodic-agent-regression.log','2d-agent-regression.log','agent-session.log','native-build.log']
    for f in logs:assert (OUT/f).exists()
    source=['include/app/cfd_open3d.h','src/app/cfd_open3d.c','src/app/cfd_open3d_observation.c','src/app/cfd_3d_session.c','src/app/cfd_3d_observation.c','include/app/cfd_3d_session.h','scripts/agent_session/cartesian3d.py','scripts/agent_session/service.py','scripts/agent_session/protocol.py','tests/cfd_open3d_test.c','tests/cfd_3d_session_test.c','tests/test_agent_open3d.py','scripts/qualify_cfd_open3d.py','scripts/verify_cfd_open3d_agent.py','scripts/audit_cfd_open3d_evidence.py','make/rules-tools.mk','make/sources-tools.mk']
    result={'schema':'physics_sim_c3d_open_completion_audit_v1','step':'C3D-6','boundary_gate_passed':True,'numerical_binary_sha256':numerical['binary_sha256'],'worker_sha256':agent['worker_sha256'],'source_sha256':{f:sha(ROOT/f) for f in source},
            'preserved_baseline_numerics':preserved,'export_readback':checks,'verified_artifact_count':artifacts,'evidence_sha256':{f:sha(OUT/f) for f in logs+['qualification.json','agent-evidence/qualification.json']},
            'committed':False,'packaged':False,'canonical_changed':False,'next_step_started':False,
            'stop_boundary':'stationary straight open Stokes duct only; no C3D-7/8, body, wake or transient outlet qualification'}
    (OUT/'completion-audit.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'passed':True,'artifacts':artifacts,'baseline_numerics_unchanged':all(preserved.values())}))

if __name__=='__main__':main()
