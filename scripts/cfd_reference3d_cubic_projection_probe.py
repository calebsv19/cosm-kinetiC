"""Read an accepted graded field; isolate native pressure trace reconstruction."""
import argparse,hashlib,json,resource,time
from pathlib import Path
import numpy as np
from skfem import MeshTet,Basis,ElementDG
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_preconditioner import array_sha
from cfd_reference3d_cubic_pressure_projection import CubicPressureProjector,project_body_pressure

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(receipt_path,n,snapshot):
    started=time.monotonic();receipt_path=receipt_path.resolve();r=json.loads(receipt_path.read_text());owner=receipt_path.parents[4]
    assert r['schema']=='physics_sim_c3d_accuracy_graded_receipt_v1' and r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None and r['wall_s']<1800 and r['peak_observed_rss_bytes']<8192*2**20
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in r['source_sha256'].items():assert sha(receipt_path.parent/'source'/q)==h==sha(owner/'scripts'/q)
    output=Path(r['command'][r['command'].index('--output')+1]);field=Path(r['command'][r['command'].index('--snapshot')+1]);original=json.loads(output.read_text())
    assert original['numerically_accepted'] and original['target']==1e-10 and original['retained_target']==1e-11 and original['final_residual']['true_residual']<1e-10 and original['flux_error']<1e-8 and original['volume_divergence_max_s_inv']<1e-8 and original['physical_energy_imbalance']<.03
    with np.load(field,allow_pickle=False) as z:
        assert bool(z['numerically_accepted']) and int(z['pressure_degree'])==3
        mesh=MeshTet(z['vertices_m'],z['tetrahedra']);pressure=z['pressure_coefficients'];lo=z['lo'];hi=z['hi'];locations=z['pressure_doflocs_m']
    assert array_sha(mesh.p,mesh.t)==original['identity']['mesh_sha256'];pb=Basis(mesh,ElementDG(ElementTetP3()),elements=np.array([0]));np.testing.assert_allclose(pb.doflocs,locations,rtol=0,atol=1e-14)
    projector=CubicPressureProjector(mesh,pb,pressure);result=project_body_pressure(projector,lo,hi,n);raw=original['pressure_force_n'][0];result.update(schema='physics_sim_c3d_cubic_pressure_projection_v1',diagnostic_accepted=True,physical_accuracy_certified=False,input_receipt=str(receipt_path),input_receipt_sha256=sha(receipt_path),input_snapshot=str(field),input_snapshot_sha256=sha(field),raw_reference_pressure_force_n=raw,trace_reconstruction_relative_difference=float(abs(result['native_trace_on_reference_n']/raw-1)),native_solved_pressure_error_measured=False)
    peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss;assert peak<1024*2**20 and time.monotonic()-started<180 and not snapshot.exists()
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    with snapshot.open('xb') as handle:np.savez_compressed(handle,pressure_average_pa=np.array([s['pressure_average_pa'] for s in result['slabs']]),raw_reference_pressure_force_n=raw,native_trace_on_reference_n=result['native_trace_on_reference_n'],diagnostic_only=True)
    peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss;assert peak<1024*2**20 and time.monotonic()-started<180
    result.update(peak_rss_bytes=peak,wall_s=time.monotonic()-started);return result
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--input-receipt',type=Path,required=True);ap.add_argument('--n',type=int,choices=(16,32),required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists();row=run(a.input_receipt,a.n,a.snapshot);a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row[k] for k in ('n','raw_reference_pressure_force_n','native_trace_on_reference_n','trace_reconstruction_relative_difference','wall_s')}),flush=True)
