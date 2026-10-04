"""Three declared profiles on both domains; no failed geometry enters CFD solve."""
import argparse,hashlib,json,resource,time
from pathlib import Path
import numpy as np
from cfd_reference3d_heldfloor6_mesh import build,quality,evaluate,SPACINGS
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
from cfd_reference3d_domain_budget import enforce_phase,PhaseResourceStopped

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate_input(p,length):
 r=json.loads(p.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['wall_s']<180 and r['peak_observed_rss_bytes']<=1800*2**20
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 out=Path(r['command'][r['command'].index('--output')+1]);snap=Path(r['command'][r['command'].index('--snapshot')+1]);row=json.loads(out.read_text())
 assert row['numerically_accepted'] and row['length']==length and row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<1e-10 and row['flux_error']<1e-8 and row['volume_divergence_max_s_inv']<1e-8 and row['physical_energy_imbalance']<.03 and row['pressure_preconditioner']['complementary_mass_inverse_scale']==10
 bundle=second_normal_tensor_mesh(length)[0];m,lo,hi,axes,n=bundle
 with np.load(snap,allow_pickle=False) as z:
  np.testing.assert_array_equal(z['vertices_m'],m.p);np.testing.assert_array_equal(z['tetrahedra'],m.t);np.testing.assert_array_equal(z['lo'],lo);np.testing.assert_array_equal(z['hi'],hi)
 return r,bundle,dict(receipt=str(p),receipt_sha256=sha(p),snapshot_sha256=sha(snap),mesh_rebuilt_bitwise=True)

def run(short_receipt,long_receipt,geometry):
 start=time.monotonic();originals={};inputs={};input_rows={}
 for length,p in ((4.,short_receipt),(8.,long_receipt)):
  r,b,inp=validate_input(p,length);inputs[str(length)]=inp;input_rows[str(length)]=r;m,lo,hi,axes,n=b;originals[str(length)]=quality(m,lo,hi,length)
 candidates=[];winner=None;winner_key=None
 for spacing in SPACINGS:
  rows={};reasons=[];bundles={}
  for length in (4.,8.):
   b=build(length,spacing);m,lo,hi,axes,n=b;metrics=quality(m,lo,hi,length);bad=evaluate(originals[str(length)],metrics,length,spacing);rows[str(length)]=dict(quality=metrics,reasons=bad,geometry_accepted=not bad,axis_nodes_m=[a.tolist() for a in axes]);bundles[str(length)]=b;reasons.extend([str(length)+': '+q for q in bad])
   print(json.dumps(dict(phase=f'profile-{spacing}-length-{length}',spacing_m=spacing,length=length,geometry_accepted=not bad,reasons=bad,quality=metrics)),flush=True);enforce_phase('geometry_candidate',resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-start)
  c=dict(spacing_m=spacing,domains=rows,geometry_accepted_on_both_lengths=not reasons,reasons=reasons,original_tetrahedron_partition_claimed=False,uniform_refinement_claimed=False);candidates.append(c)
  if not reasons:
   key=(max(rows[str(L)]['quality']['bands']['edge-0.05']['weighted_mean_shape']/originals[str(L)]['bands']['edge-0.05']['weighted_mean_shape'] for L in (4.,8.)),spacing)
   if winner is None or key<winner_key:winner=(bundles,c);winner_key=key
 selected=None
 if winner is not None:
  bundles,selected=winner;arrays={}
  for length in (4.,8.):
   m,lo,hi,axes,n=bundles[str(length)];name='L'+str(int(length));arrays.update({name+'_vertices_m':m.p,name+'_tetrahedra':m.t,name+'_lo':lo,name+'_hi':hi,name+'_axis0':axes[0],name+'_axis1':axes[1],name+'_axis2':axes[2],name+'_macro_tetrahedra':n})
  with geometry.open('xb') as f:np.savez_compressed(f,**arrays)
 for r in input_rows.values():
  for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 row=dict(schema='physics_sim_c3d_dual_domain_heldfloor6_v1',diagnostic_accepted=True,numerically_accepted=False,physical_accuracy_certified=False,numeric_factor_attempted=False,numerical_field_published=False,inputs=inputs,original_quality=originals,candidates=candidates,selected_case=selected,geometry_path=str(geometry) if selected else None,geometry_sha256=sha(geometry) if selected else None,wall_s=time.monotonic()-start,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
 enforce_phase('survey_serialization',resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-start);return row
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--short-receipt',type=Path,required=True);ap.add_argument('--long-receipt',type=Path,required=True);ap.add_argument('--geometry',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.geometry.exists()
 try:row=run(a.short_receipt,a.long_receipt,a.geometry)
 except PhaseResourceStopped as e:row=dict(diagnostic_accepted=False,numerically_accepted=False,physical_accuracy_certified=False,resource_phase_rejected=e.record)
 a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(dict(diagnostic_accepted=row['diagnostic_accepted'],selected_case=row.get('selected_case'),wall_s=row.get('wall_s'))),flush=True);raise SystemExit(0 if row['diagnostic_accepted'] else 2)
