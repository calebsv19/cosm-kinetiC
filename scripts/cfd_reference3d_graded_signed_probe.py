#!/usr/bin/env python3
"""Read one immutable accepted reference snapshot and attribute its force gap."""
import argparse
import hashlib
import json
import resource
import time
from pathlib import Path
import numpy as np
from skfem import MeshTet,Basis,ElementDG
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_p4 import ElementTetP4,require_sorted
from cfd_reference3d_quartic_pair import quartic_quadrature
from cfd_reference3d_preconditioner import array_sha
from cfd_reference3d_signed_equilibrium import diagnose_equilibrium


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def run(receipt_path,chunk_size=512,attribution_path=None):
    begin=time.monotonic();receipt=json.loads(receipt_path.read_text())
    assert receipt['returncode']==0 and receipt['stop_reason'] is None
    for path,digest in receipt['artifact_sha256'].items():assert sha(Path(path))==digest
    for name,digest in receipt['source_sha256'].items():assert sha(receipt_path.parent/'source'/name)==digest
    command=receipt['command']
    snapshot=Path(command[command.index('--snapshot')+1])
    result=Path(command[command.index('--output')+1])
    assert str(snapshot) in receipt['artifact_sha256'] and str(result) in receipt['artifact_sha256']
    original=json.loads(result.read_text());assert original['numerically_accepted']
    assert original['final_residual']['true_residual']<1e-10 and original['iterations']<=3000
    assert original['flux_error']<1e-8 and original['volume_divergence_max_s_inv']<1e-8
    assert original['physical_energy_imbalance']<.03
    assert receipt['schema']=='physics_sim_c3d_accuracy_graded_receipt_v1' and receipt['wall_s']<1800 and receipt['peak_observed_rss_bytes']<8192*1024**2
    with np.load(snapshot,allow_pickle=False) as saved:
        assert bool(saved['numerically_accepted'])
        assert int(saved['velocity_degree'])==4 and int(saved['pressure_degree'])==3
        mesh=MeshTet(saved['vertices_m'],saved['tetrahedra']);u=saved['velocity_coefficients'];p=saved['pressure_coefficients']
        lo=saved['lo'];hi=saved['hi'];length=float(saved['length']);mu=float(saved['mu'])
        expected_u_locations=saved['velocity_doflocs_m'];expected_p_locations=saved['pressure_doflocs_m']
    require_sorted(mesh);assert mesh.nelements<=120000
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    ub=Basis(mesh,ElementTetP4(),quadrature=quartic_quadrature(),elements=np.array([0]));pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=ub.quadrature,elements=np.array([0]))
    np.testing.assert_allclose(ub.doflocs,expected_u_locations,atol=1e-14)
    np.testing.assert_allclose(pb.doflocs,expected_p_locations,atol=1e-14)
    assert u.shape==(3,ub.N) and p.shape==(pb.N,) and np.all(np.isfinite(u)) and np.all(np.isfinite(p))
    assert array_sha(mesh.p,mesh.t)==original['identity']['mesh_sha256']
    print(json.dumps({'phase':'snapshot_verified','tetrahedra':mesh.nelements,'snapshot_sha256':sha(snapshot)}),flush=True)
    row=diagnose_equilibrium(mesh,ub,pb,u,p,mu,lo,hi,chunk_size,signed_output=attribution_path)
    for lift,old in zip(row['lifts'],original['consistency_diagnostics']['volume_lifts']):
        assert lift['shell_m']==old['shell_m']
        weak=lift['pressure']['weak_load_n'][0]+lift['viscous']['weak_load_n'][0]
        assert abs(weak-old['symmetric_stress_load_n'])<1e-9
        for part,key in (('pressure','pressure_force_n'),('viscous','raw_symmetric_viscous_force_n')):
            assert np.max(np.abs(np.array(lift[part]['raw_surface_load_n'])-original[key]))<1e-10
            assert np.max(np.abs(lift[part]['identity_error_n']))<1e-9
    peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    assert peak<3072*1024**2 and time.monotonic()-begin<1800
    row.update(input_complete_numerical_gates_passed=True,velocity_degree=4,pressure_degree=3,diagnostic_accepted=True,input_receipt=str(receipt_path),input_receipt_sha256=sha(receipt_path),
        input_snapshot=str(snapshot),input_snapshot_sha256=sha(snapshot),input_result_sha256=sha(result),
        original_linear_residual=original['final_residual'],original_reaction_force_n=original['reaction_force_n'],
        tetrahedra=mesh.nelements,chunk_size=chunk_size,peak_rss_bytes=peak,wall_s=time.monotonic()-begin)
    if attribution_path is not None:row.update(signed_attribution_path=str(attribution_path),signed_attribution_sha256=sha(attribution_path))
    # Recheck inputs after observation; never modify them or publish another field.
    for path,digest in receipt['artifact_sha256'].items():assert sha(Path(path))==digest
    return row


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--input-receipt',type=Path,required=True)
    ap.add_argument('--chunk-size',type=int,choices=(128,256,512,1024),default=512)
    ap.add_argument('--attribution',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.attribution.exists()
    row=run(a.input_receipt,a.chunk_size,a.attribution);a.output.write_text(json.dumps(row,indent=2)+'\n')
    print(json.dumps({'diagnostic_accepted':True,'tetrahedra':row['tetrahedra'],'wall_s':row['wall_s'],
        'volume_equilibrium_defect_l2':row['volume_strong_equilibrium_defect_l2'],
        'interior_stress_jump_l2':row['interior_stress_jump_l2']}),flush=True)
