#!/usr/bin/env python3
"""Bounded independent old/new observation equality and cost on an accepted saved field."""
import argparse,json,time,resource,hashlib
from pathlib import Path
import numpy as np
from skfem import MeshTet,Basis,ElementDG
from cfd_reference3d_p4 import ElementTetP4
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_quartic_pair import quartic_quadrature
from cfd_reference3d_chunked import volume_metrics
from cfd_reference3d_quartic_observation import quartic_consistency
from cfd_reference3d_fused_observation import observe_fused

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(receipt_path,chunk_size,attribution_path):
 begin=time.monotonic();r=json.loads(receipt_path.read_text());assert r['returncode']==0 and r['stop_reason'] is None
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 result=Path(r['command'][r['command'].index('--output')+1]);oldrow=json.loads(result.read_text());assert oldrow['numerically_accepted'] and oldrow['final_residual']['true_residual']<1e-10 and oldrow['volume_divergence_max_s_inv']<1e-8 and oldrow['flux_error']<1e-8 and oldrow['physical_energy_imbalance']<.03
 snapshot=Path(r['command'][r['command'].index('--snapshot')+1])
 with np.load(snapshot,allow_pickle=False) as z:
  assert bool(z['numerically_accepted']);mesh=MeshTet(z['vertices_m'],z['tetrahedra']);u=z['velocity_coefficients'];p=z['pressure_coefficients'];lo=z['lo'];hi=z['hi'];mu=float(z['mu'])
 def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
 mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],float(oldrow['length'])),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
 ub=Basis(mesh,ElementTetP4(),quadrature=quartic_quadrature(),elements=np.array([0]));pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=ub.quadrature,elements=np.array([0]))
 mark=time.monotonic();metrics=volume_metrics(mesh,ub,u,mu,chunk_size);metrics_s=time.monotonic()-mark
 mark=time.monotonic();checks=quartic_consistency(mesh,ub,pb,u,p,mu,lo,hi,chunk_size);consistency_s=time.monotonic()-mark
 print(json.dumps(dict(phase='old_observation_complete',metrics_s=metrics_s,consistency_s=consistency_s)),flush=True)
 mark=time.monotonic();fmetrics,fchecks=observe_fused(mesh,ub,pb,u,p,mu,lo,hi,chunk_size);fused_s=time.monotonic()-mark
 np.testing.assert_array_equal(np.asarray(metrics).view(np.uint64),np.asarray(fmetrics).view(np.uint64))
 assert json.dumps(checks,sort_keys=True,allow_nan=False)==json.dumps(fchecks,sort_keys=True,allow_nan=False)
 assert metrics[0]==oldrow['physical_dissipation_w'] and metrics[1]==oldrow['divergence_l2_s_inv_m_3_2'] and metrics[2]==oldrow['volume_divergence_max_s_inv'] and checks==oldrow['consistency_diagnostics']
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 attribution_path.write_text(json.dumps(dict(old_metrics=metrics,fused_metrics=fmetrics,old_consistency=checks,fused_consistency=fchecks),indent=2)+'\n')
 return dict(diagnostic_accepted=True,numerical_field_published=False,physical_accuracy_certified=False,input_receipt=str(receipt_path),input_receipt_sha256=sha(receipt_path),input_snapshot_sha256=sha(snapshot),input_complete_numerical_gates_passed=True,source_field_preserved=True,bitwise_observation_equality=True,old_metrics_s=metrics_s,old_consistency_s=consistency_s,old_combined_s=metrics_s+consistency_s,fused_s=fused_s,cost_ratio=fused_s/(metrics_s+consistency_s),saved_wall_s=metrics_s+consistency_s-fused_s,tetrahedra=mesh.nelements,chunk_size=chunk_size,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,wall_s=time.monotonic()-begin,signed_attribution_path=str(attribution_path),signed_attribution_sha256=sha(attribution_path))
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--input-receipt',type=Path,required=True);ap.add_argument('--chunk-size',type=int,choices=(512,),default=512);ap.add_argument('--attribution',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.attribution.exists();row=run(a.input_receipt,a.chunk_size,a.attribution);a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row),flush=True)
